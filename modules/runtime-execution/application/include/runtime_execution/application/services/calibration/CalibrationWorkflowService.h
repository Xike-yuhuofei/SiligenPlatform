#pragma once

#include "runtime_execution/contracts/system/ICalibrationExecutionPort.h"
#include "runtime_execution/contracts/system/ICalibrationResultSink.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <utility>

namespace Siligen::RuntimeExecution::Application::Services::Calibration {

using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionRequest;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionResult;
using Siligen::RuntimeExecution::Contracts::System::CalibrationExecutionState;
using Siligen::RuntimeExecution::Contracts::System::ICalibrationExecutionPort;
using Siligen::RuntimeExecution::Contracts::System::ICalibrationResultSink;
using Siligen::Shared::Types::Result;

class CalibrationWorkflowService final {
   public:
    explicit CalibrationWorkflowService(
        std::shared_ptr<ICalibrationExecutionPort> execution_port,
        std::shared_ptr<ICalibrationResultSink> result_sink) noexcept;

    CalibrationWorkflowService(const CalibrationWorkflowService&) = delete;
    CalibrationWorkflowService& operator=(const CalibrationWorkflowService&) = delete;
    CalibrationWorkflowService(CalibrationWorkflowService&&) = delete;
    CalibrationWorkflowService& operator=(CalibrationWorkflowService&&) = delete;

    Result<void> Start(const CalibrationExecutionRequest& request) noexcept;
    Result<void> ProcessNextStep() noexcept;
    Result<void> Cancel() noexcept;
    void Reset() noexcept;

    CalibrationExecutionState GetState() const noexcept {
        return current_state_;
    }

    const CalibrationExecutionRequest& GetRequest() const noexcept {
        return context_.request;
    }

    const CalibrationExecutionResult& GetResult() const noexcept {
        return context_.result;
    }

    bool IsActive() const noexcept {
        return current_state_ != CalibrationExecutionState::Idle && !IsTerminalState(current_state_);
    }

   private:
    struct CalibrationContext {
        CalibrationExecutionRequest request;
        CalibrationExecutionResult result;
        std::string error_message;
        std::chrono::steady_clock::time_point start_time{};
        std::chrono::steady_clock::time_point end_time{};
    };

    std::shared_ptr<ICalibrationExecutionPort> execution_port_;
    std::shared_ptr<ICalibrationResultSink> result_sink_;
    CalibrationExecutionState current_state_{CalibrationExecutionState::Idle};
    CalibrationContext context_{};
    std::set<std::pair<CalibrationExecutionState, CalibrationExecutionState>> valid_transitions_;

    void InitializeValidTransitions() noexcept;
    bool IsValidTransition(CalibrationExecutionState from, CalibrationExecutionState to) const noexcept;
    Result<void> TransitionTo(CalibrationExecutionState new_state) noexcept;
    Result<void> FinalizeResult(CalibrationExecutionState terminal_state, const std::string& message) noexcept;
    Result<void> FailWithError(const Siligen::Shared::Types::Error& error) noexcept;
    Result<void> ProcessValidation() noexcept;
    Result<void> ProcessExecution() noexcept;
    Result<void> ProcessVerification() noexcept;

    static bool IsTerminalState(CalibrationExecutionState state) noexcept;
    static const char* StateToString(CalibrationExecutionState state) noexcept;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Calibration
