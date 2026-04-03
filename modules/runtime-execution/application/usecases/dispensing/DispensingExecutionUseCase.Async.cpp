#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <sstream>
#include <thread>
#include <vector>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Dispensing {

namespace {

void ClampRunningProgress(uint32 total_segments, uint32& executed_segments, uint32& progress_percent) {
    if (total_segments == 0U) {
        progress_percent = 0U;
        executed_segments = 0U;
        return;
    }

    if (progress_percent >= 100U) {
        progress_percent = 99U;
    }

    if (executed_segments >= total_segments) {
        executed_segments = total_segments > 0U ? total_segments - 1U : 0U;
    }
}

constexpr auto kTaskTerminalPoll = std::chrono::milliseconds(20);
constexpr auto kCancelConfirmTimeout = std::chrono::seconds(3);
constexpr auto kInflightReconcilePoll = std::chrono::milliseconds(100);

void TrimRetainedMotionTrajectoryPoints(DispensingExecutionRequest& request) {
    auto& plan = request.execution_package.execution_plan;
    if (plan.interpolation_points.size() < 2U || plan.motion_trajectory.points.empty()) {
        return;
    }

    decltype(plan.motion_trajectory.points) empty_points;
    plan.motion_trajectory.points.swap(empty_points);
}

const char* JobStateCode(JobState state) {
    switch (state) {
        case JobState::PENDING:
            return "PENDING";
        case JobState::RUNNING:
            return "RUNNING";
        case JobState::STOPPING:
            return "STOPPING";
        case JobState::PAUSED:
            return "PAUSED";
        case JobState::COMPLETED:
            return "COMPLETED";
        case JobState::FAILED:
            return "FAILED";
        case JobState::CANCELLED:
            return "CANCELLED";
        default:
            return "UNKNOWN";
    }
}

JobState ParseJobStateForTesting(const std::string& state) {
    if (state == "pending") {
        return JobState::PENDING;
    }
    if (state == "running") {
        return JobState::RUNNING;
    }
    if (state == "stopping") {
        return JobState::STOPPING;
    }
    if (state == "paused") {
        return JobState::PAUSED;
    }
    if (state == "completed") {
        return JobState::COMPLETED;
    }
    if (state == "failed") {
        return JobState::FAILED;
    }
    if (state == "cancelled") {
        return JobState::CANCELLED;
    }
    return JobState::FAILED;
}

}  // namespace

bool DispensingExecutionUseCase::Impl::IsTerminalState(TaskState state) {
    return state == TaskState::COMPLETED || state == TaskState::FAILED || state == TaskState::CANCELLED;
}

bool DispensingExecutionUseCase::Impl::IsTerminalJobState(JobState state) {
    return state == JobState::COMPLETED || state == JobState::FAILED || state == JobState::CANCELLED;
}

TaskState DispensingExecutionUseCase::Impl::ResolveVisibleState(
    const std::shared_ptr<TaskExecutionContext>& context) {
    if (!context) {
        return TaskState::FAILED;
    }

    const auto state = context->state.load();
    if (!context->terminal_committed.load()) {
        return state;
    }

    const auto committed = context->committed_terminal_state.load();
    if (IsTerminalState(committed)) {
        return committed;
    }
    return state;
}

bool DispensingExecutionUseCase::Impl::TryCommitTerminalState(
    const std::shared_ptr<TaskExecutionContext>& context,
    TaskState terminal_state,
    const std::string& error_message) {
    if (!context || !IsTerminalState(terminal_state)) {
        return false;
    }

    bool expected = false;
    if (!context->terminal_committed.compare_exchange_strong(expected, true)) {
        return false;
    }

    context->committed_terminal_state.store(terminal_state);
    context->state.store(terminal_state);
    context->end_time = std::chrono::steady_clock::now();
    if (!error_message.empty()) {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = error_message;
    }
    return true;
}

bool DispensingExecutionUseCase::Impl::TryCommitJobTerminalState(
    const std::shared_ptr<JobExecutionContext>& context,
    JobState terminal_state,
    const std::string& error_message) {
    if (!context || !IsTerminalJobState(terminal_state)) {
        return false;
    }

    bool expected = false;
    if (!context->final_state_committed.compare_exchange_strong(expected, true)) {
        return false;
    }

    context->state.store(terminal_state);
    context->end_time = std::chrono::steady_clock::now();
    if (!error_message.empty()) {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = error_message;
    }
    return true;
}

std::shared_ptr<TaskExecutionContext> DispensingExecutionUseCase::Impl::ResolveActiveContextLocked() const {
    if (active_task_id_.empty()) {
        return nullptr;
    }
    auto it = tasks_.find(active_task_id_);
    if (it == tasks_.end()) {
        return nullptr;
    }
    return it->second;
}

void DispensingExecutionUseCase::Impl::CleanupTerminalTasksLocked() {
    auto it = tasks_.begin();
    while (it != tasks_.end()) {
        const auto& context = it->second;
        if (!context || !IsTerminalState(ResolveVisibleState(context))) {
            ++it;
            continue;
        }
        if (active_task_id_ == it->first) {
            active_task_id_.clear();
        }
        it = tasks_.erase(it);
    }
}

void DispensingExecutionUseCase::Impl::CleanupTerminalJobsLocked() {
    auto it = jobs_.begin();
    while (it != jobs_.end()) {
        const auto& context = it->second;
        if (!context || !IsTerminalJobState(context->state.load())) {
            ++it;
            continue;
        }
        if (active_job_id_ == it->first) {
            active_job_id_.clear();
        }
        it = jobs_.erase(it);
    }
}

void DispensingExecutionUseCase::Impl::JoinWorkerThread() {
    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        if (!worker_thread_.joinable()) {
            return;
        }
        worker_to_join = std::move(worker_thread_);
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }
}

void DispensingExecutionUseCase::Impl::JoinJobWorkerThread() {
    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(job_worker_mutex_);
        if (!job_worker_thread_.joinable()) {
            return;
        }
        worker_to_join = std::move(job_worker_thread_);
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }
}

void DispensingExecutionUseCase::Impl::RegisterTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
    if (!context) {
        return;
    }
    context->inflight_released.store(false);
    context->inflight_registered.store(true);
    inflight_tasks_.fetch_add(1, std::memory_order_relaxed);
}

void DispensingExecutionUseCase::Impl::ReleaseTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
    if (!context || !context->inflight_registered.load()) {
        return;
    }

    bool expected = false;
    if (!context->inflight_released.compare_exchange_strong(expected, true)) {
        return;
    }

    const auto remaining = inflight_tasks_.fetch_sub(1, std::memory_order_relaxed);
    if (remaining <= 1U) {
        std::lock_guard<std::mutex> lock(inflight_mutex_);
        inflight_cv_.notify_all();
    }
}

bool DispensingExecutionUseCase::Impl::WaitForTaskTerminalState(
    const std::shared_ptr<TaskExecutionContext>& context,
    std::chrono::milliseconds timeout,
    TaskState* terminal_state_out) const {
    if (!context) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto observed = ResolveVisibleState(context);
        if (IsTerminalState(observed)) {
            if (terminal_state_out != nullptr) {
                *terminal_state_out = observed;
            }
            return true;
        }
        std::this_thread::sleep_for(kTaskTerminalPoll);
    }

    const auto observed = ResolveVisibleState(context);
    if (IsTerminalState(observed)) {
        if (terminal_state_out != nullptr) {
            *terminal_state_out = observed;
        }
        return true;
    }
    return false;
}

bool DispensingExecutionUseCase::Impl::WaitForAllInflightTasks(
    std::chrono::milliseconds timeout,
    std::string* diagnostics_out) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::unique_lock<std::mutex> lock(inflight_mutex_);
            if (inflight_cv_.wait_for(
                    lock,
                    kInflightReconcilePoll,
                    [this]() { return inflight_tasks_.load(std::memory_order_relaxed) == 0U; })) {
                return true;
            }
        }
        ReconcileStalledInflightTasks();
    }

    ReconcileStalledInflightTasks();
    {
        std::unique_lock<std::mutex> lock(inflight_mutex_);
        if (inflight_cv_.wait_for(
                lock,
                kInflightReconcilePoll,
                [this]() { return inflight_tasks_.load(std::memory_order_relaxed) == 0U; })) {
            return true;
        }
    }
    if (diagnostics_out != nullptr) {
        *diagnostics_out = BuildInflightDiagnostics();
    }
    return inflight_tasks_.load(std::memory_order_relaxed) == 0U;
}

void DispensingExecutionUseCase::Impl::ReconcileStalledInflightTasks() {
    if (!task_scheduler_port_) {
        return;
    }

    std::vector<std::pair<TaskID, std::shared_ptr<TaskExecutionContext>>> stalled_tasks;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        stalled_tasks.reserve(tasks_.size());
        for (const auto& entry : tasks_) {
            const auto& context = entry.second;
            if (!context) {
                continue;
            }
            if (!context->inflight_registered.load() || context->inflight_released.load()) {
                continue;
            }
            if (context->execution_started.load()) {
                continue;
            }
            stalled_tasks.emplace_back(entry.first, context);
        }
    }

    using SchedulerTaskStatus = Domain::Dispensing::Ports::TaskStatus;
    for (const auto& stalled_task : stalled_tasks) {
        const auto& task_id = stalled_task.first;
        const auto& context = stalled_task.second;

        std::string scheduler_task_id;
        {
            std::lock_guard<std::mutex> context_lock(context->mutex_);
            scheduler_task_id = context->scheduler_task_id;
        }
        if (scheduler_task_id.empty()) {
            continue;
        }

        auto cancel_result = task_scheduler_port_->CancelTask(scheduler_task_id);
        if (cancel_result.IsSuccess()) {
            TryCommitTerminalState(
                context,
                TaskState::CANCELLED,
                "failure_stage=inflight_reconcile;failure_code=SCHEDULER_CANCELLED;task_id=" + task_id +
                    ";message=pending_scheduler_task_cancelled");
            ReleaseTaskInflight(context);
            continue;
        }

        auto scheduler_status_result = task_scheduler_port_->GetTaskStatus(scheduler_task_id);
        if (scheduler_status_result.IsError()) {
            if (scheduler_status_result.GetError().GetCode() == ErrorCode::NOT_FOUND) {
                TryCommitTerminalState(
                    context,
                    TaskState::FAILED,
                    "failure_stage=inflight_reconcile;failure_code=SCHEDULER_TASK_NOT_FOUND;task_id=" + task_id +
                        ";message=" + scheduler_status_result.GetError().GetMessage());
                ReleaseTaskInflight(context);
            }
            continue;
        }

        const auto scheduler_status = scheduler_status_result.Value().status;
        if (scheduler_status == SchedulerTaskStatus::CANCELLED) {
            TryCommitTerminalState(
                context,
                TaskState::CANCELLED,
                "failure_stage=inflight_reconcile;failure_code=SCHEDULER_CANCELLED;task_id=" + task_id +
                    ";message=scheduler_terminal_cancelled");
            ReleaseTaskInflight(context);
            continue;
        }
        if (scheduler_status == SchedulerTaskStatus::FAILED) {
            TryCommitTerminalState(
                context,
                TaskState::FAILED,
                "failure_stage=inflight_reconcile;failure_code=SCHEDULER_FAILED;task_id=" + task_id +
                    ";message=" + scheduler_status_result.Value().error_message);
            ReleaseTaskInflight(context);
            continue;
        }
        if (scheduler_status == SchedulerTaskStatus::COMPLETED) {
            TryCommitTerminalState(context, TaskState::COMPLETED, "");
            ReleaseTaskInflight(context);
        }
    }
}

std::string DispensingExecutionUseCase::Impl::BuildInflightDiagnostics() const {
    std::ostringstream oss;
    oss << "inflight_count=" << inflight_tasks_.load(std::memory_order_relaxed);

    std::vector<std::string> task_summaries;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        task_summaries.reserve(tasks_.size());
        for (const auto& entry : tasks_) {
            const auto& task_id = entry.first;
            const auto& context = entry.second;
            if (!context) {
                continue;
            }
            if (!context->inflight_registered.load() || context->inflight_released.load()) {
                continue;
            }

            std::string scheduler_task_id;
            {
                std::lock_guard<std::mutex> context_lock(context->mutex_);
                scheduler_task_id = context->scheduler_task_id;
            }
            task_summaries.push_back(
                "task_id=" + task_id +
                ",state=" + TaskStateToString(ResolveVisibleState(context)) +
                ",execution_started=" + (context->execution_started.load() ? std::string("true") : std::string("false")) +
                ",scheduler_task_id=" + (scheduler_task_id.empty() ? std::string("<none>") : scheduler_task_id));
        }
    }

    if (!task_summaries.empty()) {
        oss << ";tasks=";
        for (std::size_t index = 0; index < task_summaries.size(); ++index) {
            if (index > 0U) {
                oss << '|';
            }
            oss << task_summaries[index];
        }
    }
    return oss.str();
}

std::string DispensingExecutionUseCase::Impl::ReadTaskErrorMessage(
    const std::shared_ptr<TaskExecutionContext>& context) const {
    if (!context) {
        return std::string();
    }
    std::lock_guard<std::mutex> lock(context->mutex_);
    return context->error_message;
}

Result<TaskID> DispensingExecutionUseCase::Impl::ExecuteAsync(const DispensingExecutionRequest& request) {
    return ExecuteAsync(std::make_shared<DispensingExecutionRequest>(request));
}

Result<TaskID> DispensingExecutionUseCase::Impl::ExecuteAsync(SharedExecutionRequest request) {
    if (!request) {
        return Result<TaskID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "execution request is required", "DispensingExecutionUseCase"));
    }

    auto mutable_request = std::const_pointer_cast<DispensingExecutionRequest>(request);
    TrimRetainedMotionTrajectoryPoints(*mutable_request);

    auto validation = request->Validate();
    if (!validation.IsSuccess()) {
        return Result<TaskID>::Failure(validation.GetError());
    }
    auto conn_check = ValidateHardwareConnection(request->dry_run);
    if (!conn_check.IsSuccess()) {
        return Result<TaskID>::Failure(conn_check.GetError());
    }

    TaskID task_id = GenerateTaskID();
    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = task_id;
    context->request = std::move(request);
    context->state.store(TaskState::PENDING);
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        CleanupTerminalTasksLocked();
        auto active = ResolveActiveContextLocked();
        if (!active && !active_task_id_.empty()) {
            active_task_id_.clear();
        }
        if (active && !IsTerminalState(ResolveVisibleState(active))) {
            return Result<TaskID>::Failure(
                Error(ErrorCode::INVALID_STATE, "another task is already active", "DispensingExecutionUseCase"));
        }
        tasks_[task_id] = context;
        active_task_id_ = task_id;
    }

    stop_requested_.store(false);
    RegisterTaskInflight(context);
    auto runner = [this, context]() {
        struct InflightGuard {
            DispensingExecutionUseCase::Impl* self;
            std::shared_ptr<TaskExecutionContext> context;
            ~InflightGuard() {
                self->ReleaseTaskInflight(context);
            }
        } inflight_guard{this, context};

        context->execution_started.store(true);

        try {
            if (context->cancel_requested.load()) {
                TryCommitTerminalState(
                    context,
                    TaskState::CANCELLED,
                    "failure_stage=runner_precheck;failure_code=cancelled;message=执行已取消");
                return;
            }

            if (!context->terminal_committed.load()) {
                context->state.store(TaskState::RUNNING);
            }
            auto exec_result = this->ExecuteInternal(*context->request, context);

            if (context->cancel_requested.load() || stop_requested_.load()) {
                TryCommitTerminalState(
                    context,
                    TaskState::CANCELLED,
                    "failure_stage=cancel_confirm;failure_code=cancelled;message=执行已取消");
            } else if (exec_result.IsSuccess()) {
                context->result = exec_result.Value();
                context->executed_segments.store(context->result.executed_segments);
                context->total_segments.store(context->result.total_segments);
                context->reported_executed_segments.store(context->result.executed_segments);
                context->reported_progress_percent.store(100);
                TryCommitTerminalState(context, TaskState::COMPLETED, "");
            } else {
                TryCommitTerminalState(
                    context,
                    TaskState::FAILED,
                    "failure_stage=execute_internal;failure_code=" +
                        std::to_string(static_cast<int>(exec_result.GetError().GetCode())) +
                        ";message=" + exec_result.GetError().GetMessage());
            }
        } catch (const std::exception& ex) {
            TryCommitTerminalState(
                context,
                TaskState::FAILED,
                "failure_stage=runner_exception;failure_code=STD_EXCEPTION;message=" + std::string(ex.what()));
        } catch (...) {
            TryCommitTerminalState(
                context,
                TaskState::FAILED,
                "failure_stage=runner_exception;failure_code=UNKNOWN_EXCEPTION;message=unknown");
        }
    };

    bool submitted_to_scheduler = false;
    if (task_scheduler_port_) {
        task_scheduler_port_->CleanupExpiredTasks();
        auto submit_result = task_scheduler_port_->SubmitTask(runner);
        if (submit_result.IsSuccess()) {
            std::lock_guard<std::mutex> context_lock(context->mutex_);
            context->scheduler_task_id = submit_result.Value();
            submitted_to_scheduler = true;
        } else {
            SILIGEN_LOG_WARNING("任务调度器提交失败，回退为本地线程执行: " + submit_result.GetError().GetMessage());
        }
    }

    if (!submitted_to_scheduler) {
        try {
            JoinWorkerThread();
            {
                std::lock_guard<std::mutex> lock(worker_mutex_);
                worker_thread_ = std::thread(std::move(runner));
            }
        } catch (const std::exception& ex) {
            TryCommitTerminalState(
                context,
                TaskState::FAILED,
                "failure_stage=local_thread_start;failure_code=THREAD_START_FAILED;message=" + std::string(ex.what()));
            ReleaseTaskInflight(context);
            return Result<TaskID>::Failure(
                Error(
                    ErrorCode::THREAD_START_FAILED,
                    "failure_stage=local_thread_start;failure_code=THREAD_START_FAILED;message=" + std::string(ex.what()),
                    "DispensingExecutionUseCase"));
        } catch (...) {
            TryCommitTerminalState(
                context,
                TaskState::FAILED,
                "failure_stage=local_thread_start;failure_code=THREAD_START_FAILED;message=unknown");
            ReleaseTaskInflight(context);
            return Result<TaskID>::Failure(
                Error(
                    ErrorCode::THREAD_START_FAILED,
                    "failure_stage=local_thread_start;failure_code=THREAD_START_FAILED;message=unknown",
                    "DispensingExecutionUseCase"));
        }
    }

    return Result<TaskID>::Success(task_id);
}

Result<JobID> DispensingExecutionUseCase::Impl::StartJob(const RuntimeStartJobRequest& request) {
    auto validation = request.execution_request.Validate();
    if (!validation.IsSuccess()) {
        return Result<JobID>::Failure(validation.GetError());
    }
    if (request.plan_fingerprint.empty()) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "plan_fingerprint is required", "DispensingExecutionUseCase"));
    }
    if (request.target_count == 0) {
        return Result<JobID>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "target_count must be greater than 0", "DispensingExecutionUseCase"));
    }

    const auto execution_request = std::make_shared<DispensingExecutionRequest>(request.execution_request);
    TrimRetainedMotionTrajectoryPoints(*execution_request);

    auto precondition_result = ValidateExecutionPreconditions(execution_request->dry_run);
    if (precondition_result.IsError()) {
        return Result<JobID>::Failure(precondition_result.GetError());
    }

    auto context = std::make_shared<JobExecutionContext>();
    context->job_id = GenerateJobID();
    context->plan_id = request.plan_id;
    context->plan_fingerprint = request.plan_fingerprint;
    context->execution_request = execution_request;
    context->state.store(JobState::PENDING);
    context->target_count.store(request.target_count);
    context->dry_run =
        execution_request->ResolveOutputPolicy() == ProcessOutputPolicy::Inhibited;
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        CleanupTerminalJobsLocked();
        if (!active_job_id_.empty()) {
            auto active_it = jobs_.find(active_job_id_);
            if (active_it != jobs_.end() && !IsTerminalJobState(active_it->second->state.load())) {
                return Result<JobID>::Failure(
                    Error(ErrorCode::INVALID_STATE, "another job is already active", "DispensingExecutionUseCase"));
            }
        }
        jobs_[context->job_id] = context;
        active_job_id_ = context->job_id;
    }

    std::thread new_worker_thread;
    try {
        new_worker_thread = std::thread([this, context]() { RunJob(context); });
    } catch (const std::exception& ex) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto job_it = jobs_.find(context->job_id);
            if (job_it != jobs_.end() && job_it->second == context) {
                jobs_.erase(job_it);
            }
            if (active_job_id_ == context->job_id) {
                active_job_id_.clear();
            }
        }
        return Result<JobID>::Failure(
            Error(
                ErrorCode::THREAD_START_FAILED,
                "failure_stage=start_job_thread_start;failure_code=THREAD_START_FAILED;message=" +
                    std::string(ex.what()),
                "DispensingExecutionUseCase"));
    } catch (...) {
        {
            std::lock_guard<std::mutex> lock(jobs_mutex_);
            auto job_it = jobs_.find(context->job_id);
            if (job_it != jobs_.end() && job_it->second == context) {
                jobs_.erase(job_it);
            }
            if (active_job_id_ == context->job_id) {
                active_job_id_.clear();
            }
        }
        return Result<JobID>::Failure(
            Error(
                ErrorCode::THREAD_START_FAILED,
                "failure_stage=start_job_thread_start;failure_code=THREAD_START_FAILED;message=unknown",
                "DispensingExecutionUseCase"));
    }

    std::thread worker_to_join;
    {
        std::lock_guard<std::mutex> lock(job_worker_mutex_);
        if (job_worker_thread_.joinable()) {
            worker_to_join = std::move(job_worker_thread_);
        }
        job_worker_thread_ = std::move(new_worker_thread);
    }
    if (worker_to_join.joinable()) {
        worker_to_join.join();
    }

    return Result<JobID>::Success(context->job_id);
}

Result<RuntimeJobStatusResponse> DispensingExecutionUseCase::Impl::GetJobStatus(const JobID& job_id) const {
    std::shared_ptr<JobExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<RuntimeJobStatusResponse>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }
    const auto response = BuildJobStatusResponse(context);
    if (IsTerminalJobState(context->state.load())) {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it != jobs_.end() && it->second == context) {
            jobs_.erase(it);
        }
        if (active_job_id_ == job_id) {
            active_job_id_.clear();
        }
    }
    return Result<RuntimeJobStatusResponse>::Success(response);
}

Result<TaskStatusResponse> DispensingExecutionUseCase::Impl::GetTaskStatus(const TaskID& task_id) const {
    std::shared_ptr<TaskExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return Result<TaskStatusResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task not found", "DispensingExecutionUseCase"));
        }
        if (task_id != active_task_id_) {
            return Result<TaskStatusResponse>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task is not active", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    TaskStatusResponse response;
    response.task_id = task_id;
    const auto state = ResolveVisibleState(context);
    response.state = TaskStateToString(state);
    response.executed_segments = context->executed_segments.load();
    response.total_segments = context->total_segments.load();
    if (response.total_segments > 0) {
        response.progress_percent = (response.executed_segments * 100) / response.total_segments;
    }
    auto now = std::chrono::steady_clock::now();
    response.elapsed_seconds = std::chrono::duration<float>(now - context->start_time).count();
    if (state == TaskState::RUNNING && response.total_segments > 0) {
        const auto estimated_execution_ms = context->estimated_execution_ms.load();
        if (estimated_execution_ms > 0) {
            const auto elapsed_ms = static_cast<uint32>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - context->start_time).count());
            auto estimated_progress_percent = static_cast<uint32>(
                std::min<std::uint64_t>(
                    99ULL,
                    (static_cast<std::uint64_t>(elapsed_ms) * 100ULL) / estimated_execution_ms));
            auto estimated_executed_segments = static_cast<uint32>(
                (static_cast<std::uint64_t>(response.total_segments) * estimated_progress_percent + 99ULL) / 100ULL);
            if (estimated_executed_segments >= response.total_segments) {
                estimated_executed_segments = response.total_segments > 0 ? response.total_segments - 1 : 0;
            }

            response.progress_percent = std::max(response.progress_percent, estimated_progress_percent);
            response.executed_segments = std::max(response.executed_segments, estimated_executed_segments);
        }

        ClampRunningProgress(response.total_segments, response.executed_segments, response.progress_percent);
    }

    context->reported_progress_percent.store(std::max(context->reported_progress_percent.load(), response.progress_percent));
    context->reported_executed_segments.store(std::max(context->reported_executed_segments.load(), response.executed_segments));
    response.progress_percent = std::max(response.progress_percent, context->reported_progress_percent.load());
    response.executed_segments = std::max(response.executed_segments, context->reported_executed_segments.load());
    if (state == TaskState::RUNNING && response.total_segments > 0) {
        ClampRunningProgress(response.total_segments, response.executed_segments, response.progress_percent);
    }
    {
        std::lock_guard<std::mutex> context_lock(context->mutex_);
        response.error_message = context->error_message;
    }

    if (IsTerminalState(state)) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(task_id);
        if (it != tasks_.end() && it->second == context) {
            tasks_.erase(it);
        }
        if (active_task_id_ == task_id) {
            active_task_id_.clear();
        }
    }

    return Result<TaskStatusResponse>::Success(response);
}

Result<void> DispensingExecutionUseCase::Impl::CancelTask(const TaskID& task_id) {
    std::shared_ptr<TaskExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task not found", "DispensingExecutionUseCase"));
        }
        if (task_id != active_task_id_) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task is not active", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    const auto state = ResolveVisibleState(context);
    if (state == TaskState::RUNNING || state == TaskState::PENDING || state == TaskState::PAUSED) {
        context->cancel_requested.store(true);
        context->pause_requested.store(false);
        bool scheduler_cancelled_before_start = false;
        if (task_scheduler_port_) {
            std::string scheduler_task_id;
            {
                std::lock_guard<std::mutex> context_lock(context->mutex_);
                scheduler_task_id = context->scheduler_task_id;
            }
            if (!scheduler_task_id.empty()) {
                auto scheduler_cancel_result = task_scheduler_port_->CancelTask(scheduler_task_id);
                if (scheduler_cancel_result.IsSuccess()) {
                    scheduler_cancelled_before_start = true;
                    TryCommitTerminalState(
                        context,
                        TaskState::CANCELLED,
                        "failure_stage=cancel_confirm;failure_code=scheduler_cancelled;message=执行已取消");
                    ReleaseTaskInflight(context);
                }
            }
        }
        StopExecution();

        TaskState terminal_state = TaskState::PENDING;
        if (!WaitForTaskTerminalState(context, kCancelConfirmTimeout, &terminal_state)) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::MOTION_TIMEOUT,
                    "failure_stage=cancel_confirm;failure_code=MOTION_TIMEOUT;message=cancel_not_confirmed",
                    "DispensingExecutionUseCase"));
        }

        if (terminal_state == TaskState::CANCELLED) {
            return Result<void>::Success();
        }
        if (terminal_state == TaskState::COMPLETED) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_STATE,
                    "failure_stage=cancel_confirm;failure_code=TASK_COMPLETED;message=task_completed_before_cancel",
                    "DispensingExecutionUseCase"));
        }

        const auto error_message = ReadTaskErrorMessage(context);
        if (!error_message.empty()) {
            return Result<void>::Failure(
                Error(
                    ErrorCode::HARDWARE_ERROR,
                    "failure_stage=cancel_confirm;failure_code=TASK_FAILED;message=" + error_message,
                    "DispensingExecutionUseCase"));
        }

        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                std::string("failure_stage=cancel_confirm;failure_code=TASK_FAILED;message=task_failed_after_cancel;scheduler_cancelled=") +
                    (scheduler_cancelled_before_start ? "true" : "false"),
                "DispensingExecutionUseCase"));
    }

    return Result<void>::Failure(
        Error(ErrorCode::INVALID_STATE, "Task cannot be cancelled in current state", "DispensingExecutionUseCase"));
}

void DispensingExecutionUseCase::Impl::CleanupExpiredTasks() {
    if (task_scheduler_port_) {
        task_scheduler_port_->CleanupExpiredTasks();
    }

    std::lock_guard<std::mutex> lock(tasks_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto it = tasks_.begin();
    while (it != tasks_.end()) {
        auto context = it->second;
        bool should_remove = false;

        const auto state = ResolveVisibleState(context);
        if (state == TaskState::COMPLETED ||
            state == TaskState::FAILED ||
            state == TaskState::CANCELLED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                now - context->end_time).count();
            if (elapsed >= 1) {
                should_remove = true;
            }
        }

        if (state == TaskState::RUNNING || state == TaskState::PAUSED) {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
                now - context->start_time).count();
            if (elapsed >= 24) {
                SILIGEN_LOG_WARNING(
                    "failure_stage=cleanup_guard;failure_code=ACTIVE_TASK_SKIPPED;task_id=" + it->first +
                    ";state=" + TaskStateToString(state) +
                    ";message=active_task_exceeded_retention_but_kept");
            }
        }

        if (should_remove) {
            if (it->first == active_task_id_) {
                active_task_id_.clear();
            }
            it = tasks_.erase(it);
        } else {
            ++it;
        }
    }
}

TaskID DispensingExecutionUseCase::Impl::GenerateTaskID() {
    const auto seq = task_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "task-" + std::to_string(millis) + "-" + std::to_string(seq);
}

JobID DispensingExecutionUseCase::Impl::GenerateJobID() {
    const auto seq = job_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "job-" + std::to_string(millis) + "-" + std::to_string(seq);
}

std::string DispensingExecutionUseCase::Impl::TaskStateToString(TaskState state) const {
    switch (state) {
        case TaskState::PENDING:
            return "pending";
        case TaskState::RUNNING:
            return "running";
        case TaskState::PAUSED:
            return "paused";
        case TaskState::COMPLETED:
            return "completed";
        case TaskState::FAILED:
            return "failed";
        case TaskState::CANCELLED:
            return "cancelled";
        default:
            return "unknown";
    }
}

std::string DispensingExecutionUseCase::Impl::JobStateToString(JobState state) const {
    switch (state) {
        case JobState::PENDING:
            return "pending";
        case JobState::RUNNING:
            return "running";
        case JobState::STOPPING:
            return "stopping";
        case JobState::PAUSED:
            return "paused";
        case JobState::COMPLETED:
            return "completed";
        case JobState::FAILED:
            return "failed";
        case JobState::CANCELLED:
            return "cancelled";
        default:
            return "unknown";
    }
}

RuntimeJobStatusResponse DispensingExecutionUseCase::Impl::BuildJobStatusResponse(
    const std::shared_ptr<JobExecutionContext>& context) const {
    RuntimeJobStatusResponse response;
    response.job_id = context->job_id;
    response.plan_id = context->plan_id;
    response.plan_fingerprint = context->plan_fingerprint;
    response.state = JobStateToString(context->state.load());
    response.target_count = context->target_count.load();
    response.completed_count = context->completed_count.load();
    response.current_cycle = context->current_cycle.load();
    response.current_segment = context->current_segment.load();
    response.total_segments = context->total_segments.load();
    response.cycle_progress_percent = context->cycle_progress_percent.load();
    response.dry_run = context->dry_run;

    if (response.target_count > 0) {
        const std::uint64_t numerator =
            static_cast<std::uint64_t>(response.completed_count) * 100ULL + response.cycle_progress_percent;
        response.overall_progress_percent = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(100ULL, numerator / response.target_count));
    }
    if (response.state == "completed") {
        response.overall_progress_percent = 100;
        response.cycle_progress_percent = 100;
    }

    const auto end_point = IsTerminalJobState(context->state.load()) ? context->end_time : std::chrono::steady_clock::now();
    response.elapsed_seconds =
        std::chrono::duration<float>(end_point - context->start_time).count();

    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        response.error_message = context->error_message;
    }
    return response;
}

void DispensingExecutionUseCase::Impl::RunJob(const std::shared_ptr<JobExecutionContext>& context) {
    if (!context->execution_request) {
        FinalizeJob(context, JobState::FAILED, "failure_stage=job_precheck;failure_code=MISSING_REQUEST;message=execution_request_unavailable");
        return;
    }

    if (context->stop_requested.load()) {
        FinalizeJob(context, JobState::CANCELLED, "job cancelled");
        return;
    }

    const auto target_count = context->target_count.load();
    for (std::uint32_t cycle = 0; cycle < target_count; ++cycle) {
        if (context->final_state_committed.load()) {
            return;
        }
        if (context->stop_requested.load()) {
            FinalizeJob(context, JobState::CANCELLED, "job cancelled");
            return;
        }

        context->current_cycle.store(cycle + 1);
        context->current_segment.store(0);
        context->total_segments.store(0);
        context->cycle_progress_percent.store(0);

        while (context->pause_requested.load()) {
            if (context->stop_requested.load()) {
                FinalizeJob(context, JobState::CANCELLED, "job cancelled");
                return;
            }
            if (!context->final_state_committed.load()) {
                context->state.store(JobState::PAUSED);
            }
            std::this_thread::sleep_for(kInflightReconcilePoll);
        }

        auto precondition_result = ValidateExecutionPreconditions(context->execution_request->dry_run);
        if (precondition_result.IsError()) {
            FinalizeJob(context, JobState::FAILED, precondition_result.GetError().GetMessage());
            return;
        }
        if (context->stop_requested.load()) {
            FinalizeJob(context, JobState::CANCELLED, "job cancelled");
            return;
        }
        if (!context->final_state_committed.load()) {
            context->state.store(JobState::RUNNING);
        }

        auto task_result = ExecuteAsync(context->execution_request);
        if (task_result.IsError()) {
            FinalizeJob(context, JobState::FAILED, task_result.GetError().GetMessage());
            return;
        }

        const auto task_id = task_result.Value();
        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id = task_id;
        }

        bool pause_forwarded = false;
        bool cancel_forwarded = false;
        while (true) {
            if (context->final_state_committed.load()) {
                return;
            }
            if (context->stop_requested.load() && !cancel_forwarded) {
                if (!context->final_state_committed.load()) {
                    context->state.store(JobState::STOPPING);
                }
                auto cancel_result = CancelTask(task_id);
                if (cancel_result.IsError()) {
                    auto task_status_after_cancel_result = GetTaskStatus(task_id);
                    if (task_status_after_cancel_result.IsError()) {
                        FinalizeJob(
                            context,
                            JobState::FAILED,
                            "failure_stage=cancel_forward;cancel_failure_code=" +
                                std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                                ";cancel_failure_message=" + cancel_result.GetError().GetMessage() +
                                ";status_query_failure_code=" +
                                std::to_string(static_cast<int>(task_status_after_cancel_result.GetError().GetCode())) +
                                ";status_query_failure_message=" + task_status_after_cancel_result.GetError().GetMessage());
                        return;
                    }

                    const auto& task_status_after_cancel = task_status_after_cancel_result.Value();
                    if (task_status_after_cancel.state == "cancelled") {
                        FinalizeJob(context, JobState::CANCELLED, task_status_after_cancel.error_message);
                        return;
                    }
                    if (task_status_after_cancel.state == "completed") {
                        context->completed_count.store(cycle + 1);
                        context->cycle_progress_percent.store(100);
                        if (cycle + 1 >= context->target_count.load()) {
                            break;
                        }
                        FinalizeJob(context, JobState::CANCELLED, "job cancelled");
                        return;
                    }
                    if (task_status_after_cancel.state == "failed") {
                        FinalizeJob(context, JobState::FAILED, task_status_after_cancel.error_message);
                        return;
                    }
                    FinalizeJob(
                        context,
                        JobState::FAILED,
                        "failure_stage=cancel_confirm;failure_code=TASK_STILL_ACTIVE;message=" +
                            cancel_result.GetError().GetMessage() +
                            ";task_state=" + task_status_after_cancel.state);
                    return;
                }
                cancel_forwarded = true;
            }

            auto task_status_result = GetTaskStatus(task_id);
            if (task_status_result.IsError()) {
                FinalizeJob(context, JobState::FAILED, task_status_result.GetError().GetMessage());
                return;
            }
            const auto& task_status = task_status_result.Value();
            context->current_segment.store(task_status.executed_segments);
            context->total_segments.store(task_status.total_segments);
            context->cycle_progress_percent.store(task_status.progress_percent);

            if (context->pause_requested.load()) {
                if (!pause_forwarded && task_status.state == "running") {
                    auto pause_result = PauseTask(task_id);
                    if (pause_result.IsError()) {
                        FinalizeJob(context, JobState::FAILED, pause_result.GetError().GetMessage());
                        return;
                    }
                    pause_forwarded = true;
                }
            } else if (pause_forwarded && task_status.state == "paused") {
                auto resume_result = ResumeTask(task_id);
                if (resume_result.IsError()) {
                    FinalizeJob(context, JobState::FAILED, resume_result.GetError().GetMessage());
                    return;
                }
                pause_forwarded = false;
            }

            if (task_status.state == "paused") {
                if (!context->final_state_committed.load()) {
                    context->state.store(JobState::PAUSED);
                }
            } else if (task_status.state == "running" || task_status.state == "pending") {
                if (!context->final_state_committed.load()) {
                    context->state.store(context->stop_requested.load() ? JobState::STOPPING : JobState::RUNNING);
                }
            } else if (task_status.state == "completed") {
                context->completed_count.store(cycle + 1);
                context->cycle_progress_percent.store(100);
                if (context->stop_requested.load() && cycle + 1 < context->target_count.load()) {
                    FinalizeJob(context, JobState::CANCELLED, "job cancelled");
                    return;
                }
                break;
            } else if (task_status.state == "cancelled") {
                FinalizeJob(context, JobState::CANCELLED, task_status.error_message);
                return;
            } else if (task_status.state == "failed") {
                FinalizeJob(context, JobState::FAILED, task_status.error_message);
                return;
            }

            std::this_thread::sleep_for(kInflightReconcilePoll);
        }

        {
            std::lock_guard<std::mutex> lock(context->mutex_);
            context->active_task_id.clear();
        }
    }

    FinalizeJob(context, JobState::COMPLETED);
}

void DispensingExecutionUseCase::Impl::FinalizeJob(
    const std::shared_ptr<JobExecutionContext>& context,
    JobState final_state,
    const std::string& error_message) {
    if (!TryCommitJobTerminalState(context, final_state, error_message)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->active_task_id.clear();
    }
    if (final_state == JobState::COMPLETED) {
        context->cycle_progress_percent.store(100);
        context->current_cycle.store(context->target_count.load());
        context->completed_count.store(context->target_count.load());
    }
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        if (active_job_id_ == context->job_id) {
            active_job_id_.clear();
        }
    }
}

#ifdef SILIGEN_TEST_HOOKS
void DispensingExecutionUseCase::Impl::SeedJobStateForTesting(
    const RuntimeJobStatusResponse& status,
    bool pause_requested) {
    auto context = std::make_shared<JobExecutionContext>();
    auto execution_request = std::make_shared<DispensingExecutionRequest>();
    execution_request->dry_run = status.dry_run;
    context->job_id = status.job_id;
    context->plan_id = status.plan_id;
    context->plan_fingerprint = status.plan_fingerprint;
    context->execution_request = execution_request;
    context->state.store(ParseJobStateForTesting(status.state));
    context->target_count.store(status.target_count);
    context->completed_count.store(status.completed_count);
    context->current_cycle.store(status.current_cycle);
    context->current_segment.store(status.current_segment);
    context->total_segments.store(status.total_segments);
    context->cycle_progress_percent.store(status.cycle_progress_percent);
    context->pause_requested.store(pause_requested);
    context->dry_run = status.dry_run;
    context->start_time = std::chrono::steady_clock::now() -
                          std::chrono::milliseconds(static_cast<std::int64_t>(status.elapsed_seconds * 1000.0f));
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        context->error_message = status.error_message;
    }
    if (IsTerminalJobState(context->state.load())) {
        context->final_state_committed.store(true);
        context->end_time = std::chrono::steady_clock::now();
    }

    std::lock_guard<std::mutex> lock(jobs_mutex_);
    jobs_[status.job_id] = context;
}

void DispensingExecutionUseCase::Impl::SetActiveJobForTesting(const JobID& job_id) {
    std::lock_guard<std::mutex> lock(jobs_mutex_);
    active_job_id_ = job_id;
}
#endif

}  // namespace Siligen::Application::UseCases::Dispensing


