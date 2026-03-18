#include "CalibrationProcess.h"

#include "shared/types/Error.h"

namespace Siligen::Domain::Machine::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {
constexpr const char* kErrorSource = "CalibrationProcess";
}  // namespace

CalibrationProcess::CalibrationProcess(std::shared_ptr<ICalibrationDevicePort> device_port,
                                       std::shared_ptr<ICalibrationResultPort> result_port) noexcept
    : device_port_(std::move(device_port)),
      result_port_(std::move(result_port)),
      current_state_(CalibrationState::Idle) {
    InitializeValidTransitions();
}

void CalibrationProcess::InitializeValidTransitions() noexcept {
    valid_transitions_ = {
        {CalibrationState::Idle, CalibrationState::Validating},
        {CalibrationState::Validating, CalibrationState::Executing},
        {CalibrationState::Executing, CalibrationState::Verifying},
        {CalibrationState::Verifying, CalibrationState::Completed},

        {CalibrationState::Validating, CalibrationState::Canceled},
        {CalibrationState::Executing, CalibrationState::Canceled},
        {CalibrationState::Verifying, CalibrationState::Canceled},

        {CalibrationState::Validating, CalibrationState::Failed},
        {CalibrationState::Executing, CalibrationState::Failed},
        {CalibrationState::Verifying, CalibrationState::Failed},

        {CalibrationState::Completed, CalibrationState::Idle},
        {CalibrationState::Failed, CalibrationState::Idle},
        {CalibrationState::Canceled, CalibrationState::Idle},
    };
}

bool CalibrationProcess::IsValidTransition(CalibrationState from, CalibrationState to) const noexcept {
    return valid_transitions_.count({from, to}) > 0;
}

Result<void> CalibrationProcess::Start(const CalibrationRequest& request) noexcept {
    if (current_state_ != CalibrationState::Idle &&
        current_state_ != CalibrationState::Completed &&
        current_state_ != CalibrationState::Failed &&
        current_state_ != CalibrationState::Canceled) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "Calibration already running (state=" + std::string(StateToString(current_state_)) + ")",
                  kErrorSource));
    }

    if (IsTerminalState(current_state_)) {
        current_state_ = CalibrationState::Idle;
    }

    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid calibration request", kErrorSource));
    }

    context_ = CalibrationContext{};
    context_.request = request;
    context_.result = CalibrationResult{};
    context_.result.calibration_id = request.calibration_id;
    context_.start_time = std::chrono::steady_clock::now();

    return TransitionTo(CalibrationState::Validating);
}

Result<void> CalibrationProcess::ProcessNextStep() noexcept {
    switch (current_state_) {
        case CalibrationState::Validating: {
            auto result = ProcessValidation();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationState::Executing);
        }

        case CalibrationState::Executing: {
            auto result = ProcessExecution();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationState::Verifying);
        }

        case CalibrationState::Verifying: {
            auto result = ProcessVerification();
            if (result.IsError()) {
                return FailWithError(result.GetError());
            }
            return TransitionTo(CalibrationState::Completed);
        }

        case CalibrationState::Idle:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Calibration not started", kErrorSource));

        case CalibrationState::Completed:
        case CalibrationState::Failed:
        case CalibrationState::Canceled:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE,
                      "Calibration already finished (state=" + std::string(StateToString(current_state_)) + ")",
                      kErrorSource));

        default:
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_STATE, "Unknown calibration state", kErrorSource));
    }
}

Result<void> CalibrationProcess::Cancel() noexcept {
    if (current_state_ == CalibrationState::Idle) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "Calibration not started", kErrorSource));
    }

    if (IsTerminalState(current_state_)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "Calibration already finished (state=" + std::string(StateToString(current_state_)) + ")",
                  kErrorSource));
    }

    if (device_port_) {
        auto stop_result = device_port_->Stop();
        if (stop_result.IsError()) {
            return FailWithError(stop_result.GetError());
        }
    }

    return TransitionTo(CalibrationState::Canceled);
}

void CalibrationProcess::Reset() noexcept {
    current_state_ = CalibrationState::Idle;
    context_ = CalibrationContext{};
}

Result<void> CalibrationProcess::TransitionTo(CalibrationState new_state) noexcept {
    if (!IsValidTransition(current_state_, new_state)) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "Invalid state transition: " + std::string(StateToString(current_state_)) + " -> " +
                      std::string(StateToString(new_state)),
                  kErrorSource));
    }

    current_state_ = new_state;

    if (IsTerminalState(new_state)) {
        std::string message;
        switch (new_state) {
            case CalibrationState::Completed:
                message = "Calibration completed";
                break;
            case CalibrationState::Canceled:
                message = "Calibration canceled";
                break;
            case CalibrationState::Failed:
                message = context_.error_message.empty() ? "Calibration failed" : context_.error_message;
                break;
            default:
                break;
        }
        return FinalizeResult(new_state, message);
    }

    return Result<void>::Success();
}

Result<void> CalibrationProcess::FinalizeResult(CalibrationState terminal_state, const std::string& message) noexcept {
    context_.end_time = std::chrono::steady_clock::now();
    context_.result.state = terminal_state;
    context_.result.success = (terminal_state == CalibrationState::Completed);
    context_.result.message = message;
    context_.result.duration_ms = static_cast<Siligen::Shared::Types::int32>(
        std::chrono::duration_cast<std::chrono::milliseconds>(context_.end_time - context_.start_time).count());

    if (result_port_) {
        auto persist_result = result_port_->SaveResult(context_.result);
        if (persist_result.IsError()) {
            return persist_result;
        }
    }

    return Result<void>::Success();
}

Result<void> CalibrationProcess::FailWithError(const Error& error) noexcept {
    context_.error_message = error.GetMessage();
    current_state_ = CalibrationState::Failed;
    auto finalize_result = FinalizeResult(CalibrationState::Failed, context_.error_message);
    if (finalize_result.IsError()) {
        return Result<void>::Failure(finalize_result.GetError());
    }
    return Result<void>::Failure(error);
}

Result<void> CalibrationProcess::ProcessValidation() noexcept {
    if (!device_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration device port not initialized", kErrorSource));
    }

    return device_port_->Prepare(context_.request);
}

Result<void> CalibrationProcess::ProcessExecution() noexcept {
    if (!device_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration device port not initialized", kErrorSource));
    }

    return device_port_->Execute(context_.request);
}

Result<void> CalibrationProcess::ProcessVerification() noexcept {
    if (!device_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Calibration device port not initialized", kErrorSource));
    }

    return device_port_->Verify(context_.request);
}

bool CalibrationProcess::IsTerminalState(CalibrationState state) noexcept {
    return state == CalibrationState::Completed ||
           state == CalibrationState::Failed ||
           state == CalibrationState::Canceled;
}

const char* CalibrationProcess::StateToString(CalibrationState state) noexcept {
    switch (state) {
        case CalibrationState::Idle:
            return "IDLE";
        case CalibrationState::Validating:
            return "VALIDATING";
        case CalibrationState::Executing:
            return "EXECUTING";
        case CalibrationState::Verifying:
            return "VERIFYING";
        case CalibrationState::Completed:
            return "COMPLETED";
        case CalibrationState::Failed:
            return "FAILED";
        case CalibrationState::Canceled:
            return "CANCELED";
        default:
            return "UNKNOWN";
    }
}

}  // namespace Siligen::Domain::Machine::DomainServices
