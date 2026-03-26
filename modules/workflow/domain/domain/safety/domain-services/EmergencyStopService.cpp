// EmergencyStopService.cpp

#include "EmergencyStopService.h"

#include "shared/types/Types.h"

namespace Siligen::Domain::Safety::DomainServices {

EmergencyStopService::EmergencyStopService(std::shared_ptr<MotionControlService> motion_control_service,
                                           std::shared_ptr<MotionStatusService> motion_status_service,
                                           std::shared_ptr<CMPService> cmp_service,
                                           std::shared_ptr<DispenserModel> dispenser_model) noexcept
    : motion_control_service_(std::move(motion_control_service)),
      motion_status_service_(std::move(motion_status_service)),
      cmp_service_(std::move(cmp_service)),
      dispenser_model_(std::move(dispenser_model)) {}

EmergencyStopStepResult EmergencyStopService::DependencyMissingResult(const char* message) noexcept {
    EmergencyStopStepResult result;
    result.attempted = true;
    result.success = false;
    result.failure = EmergencyStopFailureKind::DependencyMissing;
    result.error = Error(ErrorCode::PORT_NOT_INITIALIZED, message, "EmergencyStopService");
    return result;
}

EmergencyStopStepResult EmergencyStopService::OperationFailedResult(const Error& error) noexcept {
    EmergencyStopStepResult result;
    result.attempted = true;
    result.success = false;
    result.failure = EmergencyStopFailureKind::OperationFailed;
    result.error = error;
    return result;
}

EmergencyStopStepResult EmergencyStopService::SuccessResult() noexcept {
    EmergencyStopStepResult result;
    result.attempted = true;
    result.success = true;
    result.failure = EmergencyStopFailureKind::None;
    return result;
}

EmergencyStopOutcome EmergencyStopService::Execute(const EmergencyStopOptions& options) noexcept {
    EmergencyStopOutcome outcome;

    if (!motion_control_service_) {
        outcome.motion_stop = DependencyMissingResult("Motion service not initialized");
    } else {
        auto result = motion_control_service_->EmergencyStop();
        if (result.IsSuccess()) {
            outcome.motion_stop = SuccessResult();
        } else {
            outcome.motion_stop = OperationFailedResult(result.GetError());
        }
    }

    if (options.disable_cmp) {
        if (!cmp_service_) {
            outcome.cmp_disable = DependencyMissingResult("CMP service not initialized");
        } else {
            auto result = cmp_service_->DisableCMP();
            if (result.IsSuccess()) {
                outcome.cmp_disable = SuccessResult();
            } else {
                outcome.cmp_disable = OperationFailedResult(result.GetError());
            }
        }
    }

    if (options.clear_task_queue) {
        if (!dispenser_model_) {
            outcome.task_queue_clear = DependencyMissingResult("Dispenser model not initialized");
        } else {
            auto size_result = dispenser_model_->GetTaskQueueSize();
            if (size_result.IsSuccess()) {
                outcome.task_queue_size_available = true;
                outcome.task_queue_size = size_result.Value();
            }

            auto clear_result = dispenser_model_->ClearAllTasks();
            if (clear_result.IsSuccess()) {
                outcome.task_queue_clear = SuccessResult();
            } else {
                outcome.task_queue_clear = OperationFailedResult(clear_result.GetError());
            }
        }
    }

    if (options.disable_hardware) {
        if (!motion_control_service_) {
            outcome.hardware_disable = DependencyMissingResult("Motion service not initialized");
        } else {
            auto result = motion_control_service_->StopAllAxes();
            if (result.IsSuccess()) {
                outcome.hardware_disable = SuccessResult();
            } else {
                outcome.hardware_disable = OperationFailedResult(result.GetError());
            }
        }
    }

    if (dispenser_model_) {
        auto result = dispenser_model_->SetState(Siligen::DispenserState::EMERGENCY_STOP);
        if (result.IsSuccess()) {
            outcome.state_update = SuccessResult();
        } else {
            outcome.state_update = OperationFailedResult(result.GetError());
        }
    }

    if (motion_status_service_) {
        auto position_result = motion_status_service_->GetCurrentPosition();
        if (position_result.IsSuccess()) {
            outcome.stop_position_available = true;
            outcome.stop_position = position_result.Value();
        }
    }

    return outcome;
}

Result<bool> EmergencyStopService::IsInEmergencyStop() const noexcept {
    if (!dispenser_model_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Dispenser model not initialized", "EmergencyStopService"));
    }

    return Result<bool>::Success(dispenser_model_->GetState() == Siligen::DispenserState::EMERGENCY_STOP);
}

Result<void> EmergencyStopService::RecoverFromEmergencyStop() noexcept {
    if (!dispenser_model_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Dispenser model not initialized", "EmergencyStopService"));
    }

    auto current = dispenser_model_->GetState();
    if (current != Siligen::DispenserState::EMERGENCY_STOP) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, "System is not in emergency stop state", "EmergencyStopService"));
    }

    if (!motion_control_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion service not initialized", "EmergencyStopService"));
    }

    auto recover_result = motion_control_service_->RecoverFromEmergencyStop();
    if (recover_result.IsError()) {
        return recover_result;
    }

    return dispenser_model_->SetState(Siligen::DispenserState::UNINITIALIZED);
}

}  // namespace Siligen::Domain::Safety::DomainServices
