#define MODULE_NAME "DispensingExecutionUseCase"

#include "DispensingExecutionUseCase.Internal.h"

#include "shared/logging/PrintfLogFormatter.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

constexpr auto kPauseTransitionTimeout = std::chrono::seconds(2);
constexpr auto kPauseTransitionPoll = std::chrono::milliseconds(50);

const char* TaskStateCode(TaskState state) {
    switch (state) {
        case TaskState::PENDING:
            return "PENDING";
        case TaskState::RUNNING:
            return "RUNNING";
        case TaskState::PAUSED:
            return "PAUSED";
        case TaskState::COMPLETED:
            return "COMPLETED";
        case TaskState::FAILED:
            return "FAILED";
        case TaskState::CANCELLED:
            return "CANCELLED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace

void DispensingExecutionUseCase::Impl::StopExecution() {
    stop_requested_.store(true);
    if (!process_port_) {
        return;
    }
    process_port_->StopExecution(&stop_requested_);
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
    if (state == JobState::STOPPING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job is stopping", "DispensingExecutionUseCase"));
    }
    if (IsTerminalJobState(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingExecutionUseCase"));
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
    if (state == JobState::STOPPING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job is stopping", "DispensingExecutionUseCase"));
    }
    if (IsTerminalJobState(state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "job already finished", "DispensingExecutionUseCase"));
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

Result<void> DispensingExecutionUseCase::Impl::StopJob(const JobID& job_id) {
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

    if (IsTerminalJobState(context->state.load())) {
        return Result<void>::Success();
    }

    context->stop_requested.store(true);
    context->pause_requested.store(false);
    context->state.store(JobState::STOPPING);

    std::string active_task_id;
    {
        std::lock_guard<std::mutex> lock(context->mutex_);
        active_task_id = context->active_task_id;
    }
    if (!active_task_id.empty()) {
        auto cancel_result = CancelTask(active_task_id);
        if (cancel_result.IsError()) {
            auto status_result = GetTaskStatus(active_task_id);
            if (status_result.IsError()) {
                if (IsTerminalJobState(context->state.load())) {
                    return Result<void>::Success();
                }
                return Result<void>::Failure(
                    Error(
                        cancel_result.GetError().GetCode(),
                        "failure_stage=stop_cancel_forward;failure_code=" +
                            std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                            ";message=" + cancel_result.GetError().GetMessage() +
                            ";status_query_failure_code=" +
                            std::to_string(static_cast<int>(status_result.GetError().GetCode())) +
                            ";status_query_failure_message=" + status_result.GetError().GetMessage(),
                        "DispensingExecutionUseCase"));
            }

            const auto& task_status = status_result.Value();
            if (task_status.state == "cancelled") {
                FinalizeJob(context, JobState::CANCELLED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "completed") {
                FinalizeJob(context, JobState::COMPLETED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "failed") {
                FinalizeJob(context, JobState::FAILED, task_status.error_message);
                return Result<void>::Failure(
                    Error(
                        ErrorCode::HARDWARE_ERROR,
                        "failure_stage=stop_cancel_forward;failure_code=TASK_FAILED;message=" +
                            task_status.error_message,
                        "DispensingExecutionUseCase"));
            }

            return Result<void>::Failure(
                Error(
                    cancel_result.GetError().GetCode(),
                    "failure_stage=stop_cancel_forward;failure_code=" +
                        std::to_string(static_cast<int>(cancel_result.GetError().GetCode())) +
                        ";message=" + cancel_result.GetError().GetMessage() +
                        ";task_state=" + task_status.state,
                    "DispensingExecutionUseCase"));
        }

        auto task_status_result = GetTaskStatus(active_task_id);
        if (task_status_result.IsSuccess()) {
            const auto& task_status = task_status_result.Value();
            if (task_status.state == "cancelled") {
                FinalizeJob(context, JobState::CANCELLED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "completed") {
                FinalizeJob(context, JobState::COMPLETED, task_status.error_message);
                return Result<void>::Success();
            }
            if (task_status.state == "failed") {
                FinalizeJob(context, JobState::FAILED, task_status.error_message);
                return Result<void>::Failure(
                    Error(
                        ErrorCode::HARDWARE_ERROR,
                        "failure_stage=stop_cancel_confirm;failure_code=TASK_FAILED;message=" +
                            task_status.error_message,
                        "DispensingExecutionUseCase"));
            }
        }

        FinalizeJob(context, JobState::CANCELLED, "failure_stage=cancel_confirm;failure_code=cancelled;message=执行已取消");
    } else {
        FinalizeJob(context, JobState::CANCELLED, "job cancelled");
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
    if (state != TaskState::RUNNING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task is not running", "DispensingExecutionUseCase"));
    }

    context->pause_requested.store(true);
    const auto deadline = std::chrono::steady_clock::now() + kPauseTransitionTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (ResolveVisibleState(context) == TaskState::PAUSED && context->pause_applied.load()) {
            return Result<void>::Success();
        }
        const auto observed_state = ResolveVisibleState(context);
        if (observed_state == TaskState::FAILED ||
            observed_state == TaskState::CANCELLED ||
            observed_state == TaskState::COMPLETED) {
            if (observed_state == TaskState::FAILED) {
                auto failure_message = ReadTaskErrorMessage(context);
                if (failure_message.empty()) {
                    failure_message = "task_failed";
                }
                return Result<void>::Failure(
                    Error(
                        ErrorCode::HARDWARE_ERROR,
                        "failure_stage=pause_confirm;failure_code=" + std::string(TaskStateCode(observed_state)) +
                            ";message=" + failure_message,
                        "DispensingExecutionUseCase"));
            }

            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_STATE,
                    "failure_stage=pause_confirm;failure_code=" + std::string(TaskStateCode(observed_state)) +
                        ";message=pause_interrupted_by_terminal_state",
                    "DispensingExecutionUseCase"));
        }
        std::this_thread::sleep_for(kPauseTransitionPoll);
    }

    return Result<void>::Failure(
        Error(
            ErrorCode::MOTION_TIMEOUT,
            "failure_stage=pause_confirm;failure_code=MOTION_TIMEOUT;message=pause_transition_timed_out",
            "DispensingExecutionUseCase"));
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
    if (state != TaskState::PAUSED) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Task is not paused", "DispensingExecutionUseCase"));
    }

    context->pause_requested.store(false);
    const auto deadline = std::chrono::steady_clock::now() + kPauseTransitionTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (ResolveVisibleState(context) == TaskState::RUNNING && !context->pause_applied.load()) {
            return Result<void>::Success();
        }
        const auto observed_state = ResolveVisibleState(context);
        if (observed_state == TaskState::FAILED ||
            observed_state == TaskState::CANCELLED ||
            observed_state == TaskState::COMPLETED) {
            if (observed_state == TaskState::FAILED) {
                auto failure_message = ReadTaskErrorMessage(context);
                if (failure_message.empty()) {
                    failure_message = "task_failed";
                }
                return Result<void>::Failure(
                    Error(
                        ErrorCode::HARDWARE_ERROR,
                        "failure_stage=resume_confirm;failure_code=" + std::string(TaskStateCode(observed_state)) +
                            ";message=" + failure_message,
                        "DispensingExecutionUseCase"));
            }

            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_STATE,
                    "failure_stage=resume_confirm;failure_code=" + std::string(TaskStateCode(observed_state)) +
                        ";message=resume_interrupted_by_terminal_state",
                    "DispensingExecutionUseCase"));
        }
        std::this_thread::sleep_for(kPauseTransitionPoll);
    }

    return Result<void>::Failure(
        Error(
            ErrorCode::MOTION_TIMEOUT,
            "failure_stage=resume_confirm;failure_code=MOTION_TIMEOUT;message=resume_transition_timed_out",
            "DispensingExecutionUseCase"));
}

}  // namespace Siligen::Application::UseCases::Dispensing

