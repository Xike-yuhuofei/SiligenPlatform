#pragma once

#include "ExecutionTransitionKernel.h"

#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <chrono>
#include <functional>
#include <thread>

namespace Siligen::Application::UseCases::Dispensing {

class ExecutionTransitionWaiter {
   public:
    template <typename TSuccessPredicate, typename TStateGetter, typename TTerminalPredicate, typename TFailureMessageReader>
    static Shared::Types::Result<void> WaitForTaskTransition(
        const char* transition_name,
        std::chrono::milliseconds timeout,
        std::chrono::milliseconds poll_interval,
        TSuccessPredicate&& is_success,
        TStateGetter&& get_state,
        TTerminalPredicate&& is_terminal,
        TFailureMessageReader&& read_failure_message,
        std::function<bool()> cancellation_predicate = {}) {
        using Siligen::Shared::Types::Error;
        using Siligen::Shared::Types::ErrorCode;
        using Siligen::Shared::Types::Result;

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto observed_state = get_state();
            if (is_success(observed_state)) {
                return Result<void>::Success();
            }
            if (is_terminal(observed_state)) {
                if (observed_state == TaskState::FAILED) {
                    auto failure_message = read_failure_message();
                    if (failure_message.empty()) {
                        failure_message = "task_failed";
                    }
                    return Result<void>::Failure(
                        Error(
                            ErrorCode::HARDWARE_ERROR,
                            std::string("failure_stage=") + transition_name +
                                "_confirm;failure_code=" + ExecutionTransitionKernel::TaskStateCode(observed_state) +
                                ";message=" + failure_message,
                            "DispensingExecutionUseCase"));
                }

                return Result<void>::Failure(
                    Error(
                        ErrorCode::INVALID_STATE,
                        std::string("failure_stage=") + transition_name +
                            "_confirm;failure_code=" + ExecutionTransitionKernel::TaskStateCode(observed_state) +
                            ";message=" + transition_name + "_interrupted_by_terminal_state",
                        "DispensingExecutionUseCase"));
            }
            if (cancellation_predicate && cancellation_predicate()) {
                return Result<void>::Failure(
                    Error(
                        ErrorCode::INVALID_STATE,
                        std::string("failure_stage=") + transition_name +
                            "_confirm;failure_code=CANCELLED;message=transition_cancelled",
                        "DispensingExecutionUseCase"));
            }
            std::this_thread::sleep_for(poll_interval);
        }

        return Result<void>::Failure(
            Error(
                ErrorCode::MOTION_TIMEOUT,
                std::string("failure_stage=") + transition_name +
                    "_confirm;failure_code=MOTION_TIMEOUT;message=" + transition_name + "_transition_timed_out",
                "DispensingExecutionUseCase"));
    }
};

}  // namespace Siligen::Application::UseCases::Dispensing
