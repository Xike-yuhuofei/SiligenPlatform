#include "MoveToPositionUseCase.h"

#include "shared/types/Error.h"

#include <chrono>
#include <thread>

namespace Siligen::Application::UseCases::Motion::PTP {

using Siligen::Shared::Interfaces::ILoggingService;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

MoveToPositionUseCase::MoveToPositionUseCase(
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionControlService> motion_control_service,
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionStatusService> motion_status_service,
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::MotionValidationService> motion_validation_service,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port,
    std::shared_ptr<ILoggingService> logging_service)
    : motion_control_service_(std::move(motion_control_service)),
      motion_status_service_(std::move(motion_status_service)),
      motion_validation_service_(std::move(motion_validation_service)),
      machine_execution_state_port_(std::move(machine_execution_state_port)),
      logging_service_(std::move(logging_service)) {}

Result<MoveToPositionResponse> MoveToPositionUseCase::Execute(const MoveToPositionRequest& request) {
    auto start_time = std::chrono::steady_clock::now();

    auto validate_result = ValidateRequest(request);
    if (validate_result.IsError()) {
        return Result<MoveToPositionResponse>::Failure(validate_result.GetError());
    }

    if (request.validate_state) {
        auto state_result = CheckMachineExecutionState();
        if (state_result.IsError()) {
            return Result<MoveToPositionResponse>::Failure(state_result.GetError());
        }
    }

    float speed = request.movement_speed;
    auto motion_result = ExecuteMotion(request.target_position, speed);
    if (motion_result.IsError()) {
        return Result<MoveToPositionResponse>::Failure(motion_result.GetError());
    }

    if (request.wait_for_completion) {
        auto wait_result = WaitForMotionCompletion(request.timeout_ms);
        if (wait_result.IsError()) {
            return Result<MoveToPositionResponse>::Failure(wait_result.GetError());
        }
    }

    MoveToPositionResponse response;
    response.motion_completed = true;

    if (motion_status_service_) {
        auto position_result = motion_status_service_->GetCurrentPosition();
        if (position_result.IsSuccess()) {
            response.actual_position = position_result.Value();
            if (motion_validation_service_) {
                auto error_result = motion_validation_service_->IsPositionErrorExceeded(
                    request.target_position,
                    response.actual_position);
                if (error_result.IsSuccess()) {
                    response.position_error = response.actual_position.DistanceTo(request.target_position);
                }
            }
        }
    }

    response.execution_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    response.status_message = "Move completed";

    return Result<MoveToPositionResponse>::Success(response);
}

Result<void> MoveToPositionUseCase::Cancel() {
    if (!motion_control_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion service not initialized", "MoveToPositionUseCase"));
    }
    return motion_control_service_->StopAllAxes();
}

Result<void> MoveToPositionUseCase::ValidateRequest(const MoveToPositionRequest& request) {
    if (!request.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid move request", "MoveToPositionUseCase"));
    }
    return Result<void>::Success();
}

Result<void> MoveToPositionUseCase::CheckMachineExecutionState() {
    if (!machine_execution_state_port_) {
        return Result<void>::Success();
    }

    auto snapshot_result = machine_execution_state_port_->ReadSnapshot();
    if (snapshot_result.IsError()) {
        return Result<void>::Failure(snapshot_result.GetError());
    }

    const auto& snapshot = snapshot_result.Value();
    if (snapshot.emergency_stopped ||
        snapshot.phase == Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase::Fault ||
        !snapshot.manual_motion_allowed) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE,
                  "Machine execution state does not allow manual motion",
                  "MoveToPositionUseCase"));
    }

    return Result<void>::Success();
}

Result<void> MoveToPositionUseCase::ExecuteMotion(const Siligen::Shared::Types::Point2D& target_position,
                                                  float speed) {
    if (!motion_validation_service_ || !motion_control_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion service not initialized", "MoveToPositionUseCase"));
    }

    auto position_check = motion_validation_service_->ValidatePosition(target_position);
    if (position_check.IsError()) {
        return position_check;
    }

    if (speed > 0.0f) {
        auto speed_check = motion_validation_service_->ValidateSpeed(speed);
        if (speed_check.IsError()) {
            return speed_check;
        }
    }

    return motion_control_service_->MoveToPosition(target_position, speed);
}

Result<void> MoveToPositionUseCase::WaitForMotionCompletion(int32 timeout_ms) {
    if (!motion_status_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion service not initialized", "MoveToPositionUseCase"));
    }

    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        auto moving_result = motion_status_service_->IsMoving();
        if (moving_result.IsError()) {
            return Result<void>::Failure(moving_result.GetError());
        }

        if (!moving_result.Value()) {
            return Result<void>::Success();
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        if (elapsed >= timeout_ms) {
            return Result<void>::Failure(
                Error(ErrorCode::TIMEOUT, "Motion completion timeout", "MoveToPositionUseCase"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

Result<void> MoveToPositionUseCase::VerifyPositionAccuracy(
    const Siligen::Shared::Types::Point2D& target_position,
    float tolerance) {
    if (!motion_status_service_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion service not initialized", "MoveToPositionUseCase"));
    }

    auto current = motion_status_service_->GetCurrentPosition();
    if (current.IsError()) {
        return Result<void>::Failure(current.GetError());
    }

    float error = current.Value().DistanceTo(target_position);
    if (error > tolerance) {
        return Result<void>::Failure(
            Error(ErrorCode::MOTION_ERROR, "Position error exceeds tolerance", "MoveToPositionUseCase"));
    }

    return Result<void>::Success();
}

void MoveToPositionUseCase::LogInfo(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogInfo(message, "MoveToPositionUseCase");
    }
}

void MoveToPositionUseCase::LogError(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogError(message, "MoveToPositionUseCase");
    }
}

void MoveToPositionUseCase::LogDebug(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogDebug(message, "MoveToPositionUseCase");
    }
}

}  // namespace Siligen::Application::UseCases::Motion::PTP

