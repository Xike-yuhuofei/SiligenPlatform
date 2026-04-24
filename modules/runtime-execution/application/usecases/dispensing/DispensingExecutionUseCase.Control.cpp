#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"
#include "ExecutionTransitionKernel.h"
#include "ExecutionTransitionWaiter.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

constexpr auto kPauseTransitionTimeout = std::chrono::seconds(2);
constexpr auto kPauseTransitionPoll = std::chrono::milliseconds(50);
constexpr int kStopSettleTimeoutMs = 2000;
constexpr int kStopSettlePollMs = 20;

ExecutionTransitionSnapshot BuildTransitionSnapshot(
    const RuntimeJobStatusResponse& status,
    bool accepted,
    std::string message = std::string()) {
    ExecutionTransitionSnapshot snapshot;
    snapshot.accepted = accepted;
    snapshot.job_id = status.job_id;
    snapshot.transition_state = status.transition_state;
    snapshot.message = message.empty() ? status.error_message : std::move(message);
    return snapshot;
}

}  // namespace

void DispensingExecutionUseCase::Impl::StopExecution() {
    stop_requested_.store(true);
    if (!process_port_) {
        return;
    }
    process_port_->StopExecution(&stop_requested_);
}

Result<void> DispensingExecutionUseCase::Impl::WaitForStopSettle(
    const std::shared_ptr<JobExecutionContext>& /*context*/) {
    if (!readiness_service_) {
        return Result<void>::Success();
    }

    auto readiness_result = readiness_service_->WaitUntilReady(
        Siligen::Application::Services::Motion::Execution::MotionReadinessQuery{},
        kStopSettleTimeoutMs,
        kStopSettlePollMs);
    if (readiness_result.IsSuccess()) {
        return Result<void>::Success();
    }

    return Result<void>::Failure(
        Error(
            readiness_result.GetError().GetCode(),
            "failure_stage=stop_settle;failure_code=" +
                std::to_string(static_cast<int>(readiness_result.GetError().GetCode())) +
                ";message=" + readiness_result.GetError().GetMessage(),
            "DispensingExecutionUseCase"));
}

void DispensingExecutionUseCase::Impl::FinalizeStoppedJob(
    const std::shared_ptr<JobExecutionContext>& context,
    const std::string& error_message) {
    auto settle_result = WaitForStopSettle(context);
    if (settle_result.IsError()) {
        FinalizeJob(context, JobState::FAILED, settle_result.GetError().GetMessage());
        return;
    }

    FinalizeJob(
        context,
        JobState::CANCELLED,
        error_message.empty() ? "failure_stage=cancel_confirm;failure_code=cancelled;message=执行已取消"
                              : error_message);
}

Result<void> DispensingExecutionUseCase::Impl::PauseJob(const JobID& job_id) {
    std::shared_ptr<JobExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == JobState::PAUSED) {
        return Result<void>::Success();
    }
    auto validation = ExecutionTransitionKernel::ValidatePauseJob(state, "DispensingExecutionUseCase");
    if (validation.IsError()) {
        return Result<void>::Failure(validation.GetError());
    }

    context->pause_requested.store(true);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto pause_result = PauseTask(active_task_id);
        if (pause_result.IsError()) {
            return pause_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingExecutionUseCase::Impl::ResumeJob(const JobID& job_id) {
    std::shared_ptr<JobExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (state == JobState::RUNNING && !context->pause_requested.load()) {
        return Result<void>::Success();
    }
    if (state != JobState::RUNNING) {
        auto validation = ExecutionTransitionKernel::ValidateResumeJob(state, "DispensingExecutionUseCase");
        if (validation.IsError()) {
            return Result<void>::Failure(validation.GetError());
        }
    }

    if (!context->execution_request) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job execution request is unavailable", "DispensingExecutionUseCase"));
    }

    auto precondition_result = ValidateExecutionPreconditions(context->execution_request->dry_run);
    if (precondition_result.IsError()) {
        return Result<void>::Failure(precondition_result.GetError());
    }

    context->pause_requested.store(false);
    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto resume_result = ResumeTask(active_task_id);
        if (resume_result.IsError()) {
            return resume_result;
        }
    }
    return Result<void>::Success();
}

Result<void> DispensingExecutionUseCase::Impl::ContinueJob(const JobID& job_id) {
    std::shared_ptr<JobExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<void>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    auto validation =
        ExecutionTransitionKernel::ValidateContinueJob(context->state.load(), "DispensingExecutionUseCase");
    if (validation.IsError()) {
        return Result<void>::Failure(validation.GetError());
    }
    if (!context->execution_request) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job execution request is unavailable", "DispensingExecutionUseCase"));
    }

    auto precondition_result = ValidateExecutionPreconditions(context->execution_request->dry_run);
    if (precondition_result.IsError()) {
        return Result<void>::Failure(precondition_result.GetError());
    }

    context->continue_requested.store(true);
    return Result<void>::Success();
}

Result<ExecutionTransitionSnapshot> DispensingExecutionUseCase::Impl::RequestJobTransition(
    const JobID& job_id,
    ExecutionTransitionState requested_transition_state) {
    if (!ExecutionTransitionKernel::IsSupportedTransitionRequest(requested_transition_state)) {
        return Result<ExecutionTransitionSnapshot>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "unsupported job transition request", "DispensingExecutionUseCase"));
    }

    std::shared_ptr<JobExecutionContext> context;
    {
        std::lock_guard<std::mutex> lock(jobs_mutex_);
        auto it = jobs_.find(job_id);
        if (it == jobs_.end()) {
            return Result<ExecutionTransitionSnapshot>::Failure(
            Error(ErrorCode::NOT_FOUND, "job not found", "DispensingExecutionUseCase"));
        }
        context = it->second;
    }

    const auto state = context->state.load();
    if (IsTerminalJobState(state)) {
        return Result<ExecutionTransitionSnapshot>::Success(BuildTransitionSnapshot(
            BuildJobStatusResponse(context),
            false,
            "job already finished"));
    }
    if (state == JobState::STOPPING) {
        context->requested_transition_state.store(requested_transition_state);
        return Result<ExecutionTransitionSnapshot>::Success(BuildTransitionSnapshot(
            BuildJobStatusResponse(context),
            true));
    }

    context->requested_transition_state.store(requested_transition_state);
    context->stop_requested.store(true);
    context->pause_requested.store(false);
    context->state.store(JobState::STOPPING);

    std::string active_task_id;
    std::shared_ptr<TaskExecutionContext> active_task_context;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        std::lock_guard<std::mutex> lock(tasks_mutex_);
        auto task_it = tasks_.find(active_task_id);
        if (task_it != tasks_.end()) {
            active_task_context = task_it->second;
        }
    }

    if (!active_task_id.empty()) {
        auto cancel_result = CancelTask(active_task_id);
        if (cancel_result.IsError()) {
            if (active_task_context) {
                const auto task_state = ResolveVisibleState(active_task_context);
                if (task_state == TaskState::CANCELLED || task_state == TaskState::COMPLETED) {
                    return Result<ExecutionTransitionSnapshot>::Success(BuildTransitionSnapshot(
                        BuildJobStatusResponse(context),
                        true));
                }
                if (task_state == TaskState::FAILED) {
                    auto failure_message = ReadTaskErrorMessage(active_task_context);
                    if (failure_message.empty()) {
                        failure_message = cancel_result.GetError().GetMessage();
                    }
                    return Result<ExecutionTransitionSnapshot>::Failure(
                        Error(
                            ErrorCode::HARDWARE_ERROR,
                            "failure_stage=stop_cancel_forward;failure_code=TASK_FAILED;message=" +
                                failure_message,
                            "DispensingExecutionUseCase"));
                }
            }

            if (IsTerminalJobState(context->state.load())) {
                return Result<ExecutionTransitionSnapshot>::Success(BuildTransitionSnapshot(
                    BuildJobStatusResponse(context),
                    false,
                    "job already finished"));
            }

            return Result<ExecutionTransitionSnapshot>::Failure(
                Error(
                    cancel_result.GetError().GetCode(),
                    "failure_stage=stop_cancel_forward;failure_code=" +
                        std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                        ";message=" + cancel_result.GetError().GetMessage(),
                    "DispensingExecutionUseCase"));
        }
    }

    return Result<ExecutionTransitionSnapshot>::Success(BuildTransitionSnapshot(
        BuildJobStatusResponse(context),
        true));
}

Result<void> DispensingExecutionUseCase::Impl::StopJob(const JobID& job_id) {
    auto transition_result = RequestJobTransition(job_id, ExecutionTransitionState::STOPPING);
    if (transition_result.IsError()) {
        return Result<void>::Failure(transition_result.GetError());
    }

    return Result<void>::Success();
}

Result<void> DispensingExecutionUseCase::Impl::PauseTask(const TaskID& task_id) {
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
    if (state == TaskState::PAUSED) {
        return Result<void>::Success();
    }
    auto validation = ExecutionTransitionKernel::ValidatePauseTask(state, "DispensingExecutionUseCase");
    if (validation.IsError()) {
        return Result<void>::Failure(validation.GetError());
    }

    context->pause_requested.store(true);
    return ExecutionTransitionWaiter::WaitForTaskTransition(
        "pause",
        kPauseTransitionTimeout,
        kPauseTransitionPoll,
        [context](TaskState observed_state) {
            return observed_state == TaskState::PAUSED && context->pause_applied.load();
        },
        [context, this]() { return ResolveVisibleState(context); },
        [this](TaskState observed_state) { return IsTerminalState(observed_state); },
        [context, this]() { return ReadTaskErrorMessage(context); });
}

Result<void> DispensingExecutionUseCase::Impl::ResumeTask(const TaskID& task_id) {
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
    if (state == TaskState::RUNNING && !context->pause_applied.load()) {
        return Result<void>::Success();
    }
    auto validation = ExecutionTransitionKernel::ValidateResumeTask(state, "DispensingExecutionUseCase");
    if (validation.IsError()) {
        return Result<void>::Failure(validation.GetError());
    }

    context->pause_requested.store(false);
    return ExecutionTransitionWaiter::WaitForTaskTransition(
        "resume",
        kPauseTransitionTimeout,
        kPauseTransitionPoll,
        [context](TaskState observed_state) {
            return observed_state == TaskState::RUNNING && !context->pause_applied.load();
        },
        [context, this]() { return ResolveVisibleState(context); },
        [this](TaskState observed_state) { return IsTerminalState(observed_state); },
        [context, this]() { return ReadTaskErrorMessage(context); });
}

}  // namespace Siligen::Application::UseCases::Dispensing

