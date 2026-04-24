#pragma once

#include "DispensingExecutionUseCase.Internal.h"

namespace Siligen::Application::UseCases::Dispensing {

class ExecutionTransitionKernel {
   public:
    static bool IsSupportedTransitionRequest(ExecutionTransitionState state) noexcept {
        return state == ExecutionTransitionState::STOPPING ||
               state == ExecutionTransitionState::CANCELING;
    }

    static const char* TaskStateCode(TaskState state) noexcept {
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

    static ExecutionTransitionState ResolveJobTransitionState(
        JobState state,
        ExecutionTransitionState requested_transition_state,
        bool pause_requested) noexcept {
        switch (state) {
            case JobState::PENDING:
                return ExecutionTransitionState::PENDING;
            case JobState::RUNNING:
                if (pause_requested) {
                    return ExecutionTransitionState::PAUSING;
                }
                return ExecutionTransitionState::RUNNING;
            case JobState::WAITING_CONTINUE:
                return ExecutionTransitionState::AWAITING_CONTINUE;
            case JobState::STOPPING:
                return requested_transition_state == ExecutionTransitionState::CANCELING
                    ? ExecutionTransitionState::CANCELING
                    : ExecutionTransitionState::STOPPING;
            case JobState::PAUSED:
                return ExecutionTransitionState::PAUSED;
            case JobState::COMPLETED:
                return ExecutionTransitionState::COMPLETED;
            case JobState::FAILED:
                return ExecutionTransitionState::FAILED;
            case JobState::CANCELLED:
                return ExecutionTransitionState::CANCELLED;
            default:
                return ExecutionTransitionState::UNKNOWN;
        }
    }

    static Shared::Types::Result<void> ValidatePauseJob(
        JobState state,
        const char* error_source) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        if (state == JobState::PAUSED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is already paused", error_source));
        }
        if (state == JobState::WAITING_CONTINUE) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is waiting for continue", error_source));
        }
        if (state == JobState::STOPPING) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is stopping", error_source));
        }
        if (state == JobState::COMPLETED || state == JobState::FAILED || state == JobState::CANCELLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job already finished", error_source));
        }
        if (state != JobState::RUNNING) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is not running", error_source));
        }
        return Result<void>::Success();
    }

    static Shared::Types::Result<void> ValidateResumeJob(
        JobState state,
        const char* error_source) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        if (state == JobState::WAITING_CONTINUE) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is waiting for continue", error_source));
        }
        if (state == JobState::STOPPING) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is stopping", error_source));
        }
        if (state == JobState::COMPLETED || state == JobState::FAILED || state == JobState::CANCELLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job already finished", error_source));
        }
        if (state != JobState::PAUSED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is not paused", error_source));
        }
        return Result<void>::Success();
    }

    static Shared::Types::Result<void> ValidateContinueJob(
        JobState state,
        const char* error_source) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        if (state == JobState::STOPPING) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is stopping", error_source));
        }
        if (state == JobState::COMPLETED || state == JobState::FAILED || state == JobState::CANCELLED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job already finished", error_source));
        }
        if (state != JobState::WAITING_CONTINUE) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "job is not waiting for continue", error_source));
        }
        return Result<void>::Success();
    }

    static Shared::Types::Result<void> ValidatePauseTask(
        TaskState state,
        const char* error_source) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        if (state != TaskState::RUNNING) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task is not running", error_source));
        }
        return Result<void>::Success();
    }

    static Shared::Types::Result<void> ValidateResumeTask(
        TaskState state,
        const char* error_source) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        if (state != TaskState::PAUSED) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Task is not paused", error_source));
        }
        return Result<void>::Success();
    }
};

}  // namespace Siligen::Application::UseCases::Dispensing
