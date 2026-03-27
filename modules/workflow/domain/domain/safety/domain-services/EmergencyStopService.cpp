// EmergencyStopService.cpp

#include "EmergencyStopService.h"

#include "shared/types/Types.h"

namespace Siligen::Domain::Safety::DomainServices {

EmergencyStopService::EmergencyStopService(std::shared_ptr<MotionControlService> motion_control_service,
                                           std::shared_ptr<MotionStatusService> motion_status_service,
                                           std::shared_ptr<CMPService> cmp_service,
                                           std::shared_ptr<IMachineExecutionStatePort> machine_execution_state_port) noexcept
    : motion_control_service_(std::move(motion_control_service)),
      motion_status_service_(std::move(motion_status_service)),
      cmp_service_(std::move(cmp_service)),
      machine_execution_state_port_(std::move(machine_execution_state_port)) {}

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
        if (!machine_execution_state_port_) {
            outcome.task_queue_clear = DependencyMissingResult("Machine execution state port not initialized");
        } else {
            auto snapshot_result = machine_execution_state_port_->ReadSnapshot();
            if (snapshot_result.IsSuccess()) {
                outcome.task_queue_size_available = true;
                outcome.task_queue_size = snapshot_result.Value().pending_task_count;
            }

            auto clear_result = machine_execution_state_port_->ClearPendingTasks();
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

    if (machine_execution_state_port_) {
        auto result = machine_execution_state_port_->TransitionToEmergencyStop();
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
    if (!machine_execution_state_port_) {
        return Result<bool>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Machine execution state port not initialized", "EmergencyStopService"));
    }

    auto snapshot_result = machine_execution_state_port_->ReadSnapshot();
    if (snapshot_result.IsError()) {
        return Result<bool>::Failure(snapshot_result.GetError());
    }
    return Result<bool>::Success(snapshot_result.Value().emergency_stopped);
}

Result<void> EmergencyStopService::RecoverFromEmergencyStop() noexcept {
    if (!machine_execution_state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Machine execution state port not initialized", "EmergencyStopService"));
    }

    auto snapshot_result = machine_execution_state_port_->ReadSnapshot();
    if (snapshot_result.IsError()) {
        return Result<void>::Failure(snapshot_result.GetError());
    }
    if (!snapshot_result.Value().emergency_stopped) {
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

    return machine_execution_state_port_->RecoverToUninitialized();
}

}  // namespace Siligen::Domain::Safety::DomainServices
