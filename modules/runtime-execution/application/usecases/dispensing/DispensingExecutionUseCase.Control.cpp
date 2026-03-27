#define MODULE_NAME "DispensingExecutionUseCase"

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include "domain/dispensing/domain-services/DispensingProcessService.h"
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

void DispensingExecutionUseCase::StopExecution() {
    stop_requested_.store(true);
    if (!process_service_) {
        return;
    }
    process_service_->StopExecution(&stop_requested_);
}

Result<void> DispensingExecutionUseCase::PauseTask(const TaskID& task_id) {
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

Result<void> DispensingExecutionUseCase::ResumeTask(const TaskID& task_id) {
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

