#define MODULE_NAME "DispensingExecutionUseCase"

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

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

}  // namespace

bool DispensingExecutionUseCase::IsTerminalState(TaskState state) {
    return state == TaskState::COMPLETED || state == TaskState::FAILED || state == TaskState::CANCELLED;
}

TaskState DispensingExecutionUseCase::ResolveVisibleState(const std::shared_ptr<TaskExecutionContext>& context) {
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

bool DispensingExecutionUseCase::TryCommitTerminalState(
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

std::shared_ptr<TaskExecutionContext> DispensingExecutionUseCase::ResolveActiveContextLocked() const {
    if (active_task_id_.empty()) {
        return nullptr;
    }
    auto it = tasks_.find(active_task_id_);
    if (it == tasks_.end()) {
        return nullptr;
    }
    return it->second;
}

void DispensingExecutionUseCase::JoinWorkerThread() {
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

void DispensingExecutionUseCase::RegisterTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
    if (!context) {
        return;
    }
    context->inflight_released.store(false);
    context->inflight_registered.store(true);
    inflight_tasks_.fetch_add(1, std::memory_order_relaxed);
}

void DispensingExecutionUseCase::ReleaseTaskInflight(const std::shared_ptr<TaskExecutionContext>& context) {
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

bool DispensingExecutionUseCase::WaitForTaskTerminalState(
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

bool DispensingExecutionUseCase::WaitForAllInflightTasks(
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

void DispensingExecutionUseCase::ReconcileStalledInflightTasks() {
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

std::string DispensingExecutionUseCase::BuildInflightDiagnostics() const {
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

std::string DispensingExecutionUseCase::ReadTaskErrorMessage(const std::shared_ptr<TaskExecutionContext>& context) const {
    if (!context) {
        return std::string();
    }
    std::lock_guard<std::mutex> lock(context->mutex_);
    return context->error_message;
}

Result<TaskID> DispensingExecutionUseCase::ExecuteAsync(const DispensingMVPRequest& request) {
    auto validation = request.Validate();
    if (!validation.IsSuccess()) {
        return Result<TaskID>::Failure(validation.GetError());
    }
    auto conn_check = ValidateHardwareConnection();
    if (!conn_check.IsSuccess()) {
        return Result<TaskID>::Failure(conn_check.GetError());
    }

    TaskID task_id = GenerateTaskID();
    auto context = std::make_shared<TaskExecutionContext>();
    context->task_id = task_id;
    context->request = request;
    context->state.store(TaskState::PENDING);
    context->start_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
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
            DispensingExecutionUseCase* self;
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
            auto exec_result = this->ExecuteInternal(context->request, context);

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

Result<TaskStatusResponse> DispensingExecutionUseCase::GetTaskStatus(const TaskID& task_id) const {
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

    const auto& context = it->second;
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

    return Result<TaskStatusResponse>::Success(response);
}

Result<void> DispensingExecutionUseCase::CancelTask(const TaskID& task_id) {
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

void DispensingExecutionUseCase::CleanupExpiredTasks() {
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

TaskID DispensingExecutionUseCase::GenerateTaskID() {
    const auto seq = task_sequence_.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return "task-" + std::to_string(millis) + "-" + std::to_string(seq);
}

std::string DispensingExecutionUseCase::TaskStateToString(TaskState state) const {
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

}  // namespace Siligen::Application::UseCases::Dispensing


