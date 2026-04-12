#include "TcpMotionFacade.h"

#include <utility>

namespace Siligen::Application::Facades::Tcp {

TcpMotionFacade::TcpMotionFacade(
    std::shared_ptr<UseCases::Motion::MotionControlUseCase> motion_control_use_case,
    std::shared_ptr<UseCases::Motion::Safety::MotionSafetyUseCase> motion_safety_use_case,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> hardware_connection_port)
    : motion_control_use_case_(std::move(motion_control_use_case)),
      motion_safety_use_case_(std::move(motion_safety_use_case)),
      hardware_connection_port_(std::move(hardware_connection_port)) {}

Shared::Types::Result<UseCases::Motion::Homing::HomeAxesResponse> TcpMotionFacade::Home(
    const UseCases::Motion::Homing::HomeAxesRequest& request) {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<UseCases::Motion::Homing::HomeAxesResponse>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->Home(request);
}

Shared::Types::Result<UseCases::Motion::Homing::EnsureAxesReadyZeroResponse> TcpMotionFacade::EnsureAxesReadyZero(
    const UseCases::Motion::Homing::EnsureAxesReadyZeroRequest& request) {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<UseCases::Motion::Homing::EnsureAxesReadyZeroResponse>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->EnsureAxesReadyZero(request);
}

Shared::Types::Result<bool> TcpMotionFacade::IsAxisHomed(Shared::Types::LogicalAxisId axis) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->IsAxisHomed(axis);
}

Shared::Types::Result<void> TcpMotionFacade::ExecutePointToPointMotion(
    const UseCases::Motion::Manual::ManualMotionCommand& command,
    bool invalidate_homing) {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->ExecutePointToPointMotion(command, invalidate_homing);
}

Shared::Types::Result<void> TcpMotionFacade::StartJog(
    Shared::Types::LogicalAxisId axis,
    int16 direction,
    float32 velocity) {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->StartJog(axis, direction, velocity);
}

Shared::Types::Result<void> TcpMotionFacade::StopJog(Shared::Types::LogicalAxisId axis) {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->StopJog(axis);
}

Shared::Types::Result<void> TcpMotionFacade::StopAllAxes(bool immediate) {
    if (!motion_safety_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionSafetyUseCase not available"));
    }
    return motion_safety_use_case_->StopAllAxes(immediate);
}

Shared::Types::Result<Domain::Motion::Ports::MotionStatus> TcpMotionFacade::GetAxisMotionStatus(
    Shared::Types::LogicalAxisId axis) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<Domain::Motion::Ports::MotionStatus>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetAxisMotionStatus(axis);
}

Shared::Types::Result<std::vector<Domain::Motion::Ports::MotionStatus>> TcpMotionFacade::GetAllAxesMotionStatus() const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetAllAxesMotionStatus();
}

Shared::Types::Result<Point2D> TcpMotionFacade::GetCurrentPosition() const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<Point2D>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetCurrentPosition();
}

Shared::Types::Result<Domain::Motion::Ports::CoordinateSystemStatus> TcpMotionFacade::GetCoordinateSystemStatus(
    int16 coord_sys) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<Domain::Motion::Ports::CoordinateSystemStatus>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetCoordinateSystemStatus(coord_sys);
}

Shared::Types::Result<uint32> TcpMotionFacade::GetInterpolationBufferSpace(int16 coord_sys) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<uint32>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetInterpolationBufferSpace(coord_sys);
}

Shared::Types::Result<uint32> TcpMotionFacade::GetLookAheadBufferSpace(int16 coord_sys) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<uint32>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->GetLookAheadBufferSpace(coord_sys);
}

Shared::Types::Result<bool> TcpMotionFacade::ReadLimitStatus(
    Shared::Types::LogicalAxisId axis,
    bool positive) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->ReadLimitStatus(axis, positive);
}

Shared::Types::Result<bool> TcpMotionFacade::ReadServoAlarmStatus(Shared::Types::LogicalAxisId axis) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->ReadServoAlarmStatus(axis);
}

Shared::Types::Result<Siligen::Application::Services::Motion::Execution::MotionReadinessResult>
TcpMotionFacade::EvaluateMotionReadiness(
    const Siligen::Application::Services::Motion::Execution::MotionReadinessQuery& query) const {
    if (!motion_control_use_case_) {
        return Shared::Types::Result<Siligen::Application::Services::Motion::Execution::MotionReadinessResult>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionControlUseCase not available"));
    }
    return motion_control_use_case_->EvaluateMotionReadiness(query);
}

bool TcpMotionFacade::HasHardwareConnectionPort() const {
    return static_cast<bool>(hardware_connection_port_);
}

Siligen::Device::Contracts::State::DeviceConnectionSnapshot TcpMotionFacade::GetHardwareConnectionInfo() const {
    if (!hardware_connection_port_) {
        return {};
    }
    auto result = hardware_connection_port_->ReadConnection();
    return result.IsSuccess() ? result.Value() : Siligen::Device::Contracts::State::DeviceConnectionSnapshot{};
}

Siligen::Device::Contracts::State::HeartbeatSnapshot TcpMotionFacade::GetHeartbeatStatus() const {
    if (!hardware_connection_port_) {
        return {};
    }
    return hardware_connection_port_->ReadHeartbeat();
}

bool TcpMotionFacade::IsHardwareReadyForMotion() const {
    if (!hardware_connection_port_) {
        return true;
    }

    const auto connection_info = GetHardwareConnectionInfo();
    const auto heartbeat_status = GetHeartbeatStatus();
    return connection_info.IsConnected() && !heartbeat_status.is_degraded;
}

}  // namespace Siligen::Application::Facades::Tcp
