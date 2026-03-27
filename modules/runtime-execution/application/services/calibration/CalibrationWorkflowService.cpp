#include "runtime_execution/application/services/calibration/CalibrationWorkflowService.h"

namespace Siligen::RuntimeExecution::Application::Services::Calibration {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {
constexpr const char* kErrorSource = "CalibrationWorkflowService";
}  // namespace

CalibrationWorkflowService::CalibrationWorkflowService(
    std::shared_ptr<ICalibrationExecutionPort> execution_port,
    std::shared_ptr<ICalibrationResultSink> result_sink) noexcept
    : execution_port_(std::move(execution_port)),
      result_sink_(std::move(result_sink)),
      current_state_(CalibrationExecutionState::Idle) {
    InitializeValidTransitions();
}

void CalibrationWorkflowService::InitializeValidTransitions() noexcept {
    valid_transitions_ = {
        {CalibrationExecutionState::Idle, CalibrationExecutionState::Validating},
        {CalibrationExecutionState::Validating, CalibrationExecutionState::Executing},
        {CalibrationExecutionState::Executing, CalibrationExecutionState::Verifying},
        {CalibrationExecutionState::Verifying, CalibrationExecutionState::Completed},
        {CalibrationExecutionState::Validating, CalibrationExecutionState::Canceled},
        {CalibrationExecutionState::Executing, CalibrationExecutionState::Canceled},
        {CalibrationExecutionState::Verifying, CalibrationExecutionState::Canceled},
        {CalibrationExecutionState::Validating, CalibrationExecutionState::Failed},
        {CalibrationExecutionState::Executing, CalibrationExecutionState::Failed},
        {CalibrationExecutionState::Verifying, CalibrationExecutionState::Failed},
        {CalibrationExecutionState::Completed, CalibrationExecutionState::Idle},
        {CalibrationExecutionState::Failed, CalibrationExecutionState::Idle},
        {CalibrationExecutionState::Canceled, CalibrationExecutionState::Idle},
    };
}

bool CalibrationWorkflowService::IsValidTransition(
    CalibrationExecutionState from,
    CalibrationExecutionState to) const noexcept {
    return valid_transitions_.count({from, to}) > 0;
}

Result<void> CalibrationWorkflowService::Start(const CalibrationExecutionRequest& request) noexcept {
    if (current_state_ != CalibrationExecutionState::Idle &&
        current_state_ != CalibrationExecutionState::Completed &&
        current_state_ != CalibrationExecutionState::Failed &&
        current_state_ != CalibrationExecutionState::Canceled) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "Calibration already running (state=" + std::string(StateToString(current_state_)) + ")",
                kErrorSource));
    }

    if (IsTerminalState(current_state_)) {
        current_state_ = CalibrationExecutionState::Idle;
    }

    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid calibration request", kErrorSource));
    }

    context_ = CalibrationContext{};
    context_.request = request;
    context_.result = CalibrationExecutionResult{};
    context_.result.calibration_id = request.calibration_id;
    context_.start_time = std::chrono::steady_clock::now();

    return TransitionTo(CalibrationExecutionState::Validating);
}

Result<void> CalibrationWorkflowService::ProcessNextStep() noexcept {
    switch (current_state_) {
        case CalibrationExecutionState::Validating: {
            auto result = ProcessValidation();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationExecutionState::Executing);
        }
        case CalibrationExecutionState::Executing: {
            auto result = ProcessExecution();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationExecutionState::Verifying);
        }
        case CalibrationExecutionState::Verifying: {
            auto result = ProcessVerification();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationExecutionState::Completed);
        }
        case CalibrationExecutionState::Idle:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Calibration not started", kErrorSource));
        case CalibrationExecutionState::Completed:
        case CalibrationExecutionState::Failed:
        case CalibrationExecutionState::Canceled:
            return Result<void>::Failure(
                Error(
                    ErrorCode::INVALID_STATE,
                    "Calibration already finished (state=" + std::string(StateToString(current_state_)) + ")",
                    kErrorSource));
        default:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Unknown calibration state", kErrorSource));
    }
}

Result<void> CalibrationWorkflowService::Cancel() noexcept {
    if (current_state_ == CalibrationExecutionState::Idle) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Calibration not started", kErrorSource));
    }
    if (IsTerminalState(current_state_)) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "Calibration already finished (state=" + std::string(StateToString(current_state_)) + ")",
                kErrorSource));
    }

    if (execution_port_) {
        auto stop_result = execution_port_->Stop();
        if (stop_result.IsError()) {
            return FailWithError(stop_result.GetError());
        }
    }

    return TransitionTo(CalibrationExecutionState::Canceled);
}

void CalibrationWorkflowService::Reset() noexcept {
    current_state_ = CalibrationExecutionState::Idle;
    context_ = CalibrationContext{};
}

Result<void> CalibrationWorkflowService::TransitionTo(CalibrationExecutionState new_state) noexcept {
    if (!IsValidTransition(current_state_, new_state)) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "Invalid state transition: " + std::string(StateToString(current_state_)) + " -> " +
                    std::string(StateToString(new_state)),
                kErrorSource));
    }

    current_state_ = new_state;
    if (!IsTerminalState(new_state)) {
        return Result<void>::Success();
    }

    std::string message;
    switch (new_state) {
        case CalibrationExecutionState::Completed:
            message = "Calibration completed";
            break;
        case CalibrationExecutionState::Canceled:
            message = "Calibration canceled";
            break;
        case CalibrationExecutionState::Failed:
            message = context_.error_message.empty() ? "Calibration failed" : context_.error_message;
            break;
        default:
            break;
    }
    return FinalizeResult(new_state, message);
}

Result<void> CalibrationWorkflowService::FinalizeResult(
    CalibrationExecutionState terminal_state,
    const std::string& message) noexcept {
    context_.end_time = std::chrono::steady_clock::now();
    context_.result.state = terminal_state;
    context_.result.success = terminal_state == CalibrationExecutionState::Completed;
    context_.result.message = message;
    context_.result.duration_ms = static_cast<Siligen::Shared::Types::int32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(context_.end_time - context_.start_time).count());

    if (result_sink_) {
        auto persist_result = result_sink_->SaveResult(context_.result);
        if (persist_result.IsError()) {
            return persist_result;
        }
    }

    return Result<void>::Success();
}

Result<void> CalibrationWorkflowService::FailWithError(const Error& error) noexcept {
    context_.error_message = error.GetMessage();
    current_state_ = CalibrationExecutionState::Failed;
    auto finalize_result = FinalizeResult(CalibrationExecutionState::Failed, context_.error_message);
    if (finalize_result.IsError()) {
        return Result<void>::Failure(finalize_result.GetError());
    }
    return Result<void>::Failure(error);
}

Result<void> CalibrationWorkflowService::ProcessValidation() noexcept {
    if (!execution_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration execution port not initialized", kErrorSource));
    }
    return execution_port_->Prepare(context_.request);
}

Result<void> CalibrationWorkflowService::ProcessExecution() noexcept {
    if (!execution_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration execution port not initialized", kErrorSource));
    }
    return execution_port_->Execute(context_.request);
}

Result<void> CalibrationWorkflowService::ProcessVerification() noexcept {
    if (!execution_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration execution port not initialized", kErrorSource));
    }
    return execution_port_->Verify(context_.request);
}

bool CalibrationWorkflowService::IsTerminalState(CalibrationExecutionState state) noexcept {
    return state == CalibrationExecutionState::Completed ||
           state == CalibrationExecutionState::Failed ||
           state == CalibrationExecutionState::Canceled;
}

const char* CalibrationWorkflowService::StateToString(CalibrationExecutionState state) noexcept {
    switch (state) {
        case CalibrationExecutionState::Idle:
            return "IDLE";
        case CalibrationExecutionState::Validating:
            return "VALIDATING";
        case CalibrationExecutionState::Executing:
            return "EXECUTING";
        case CalibrationExecutionState::Verifying:
            return "VERIFYING";
        case CalibrationExecutionState::Completed:
            return "COMPLETED";
        case CalibrationExecutionState::Failed:
            return "FAILED";
        case CalibrationExecutionState::Canceled:
            return "CANCELED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Calibration
