#include "TcpMotionFacade.h"

namespace Siligen::Application::Facades::Tcp {

TcpMotionFacade::TcpMotionFacade(
    std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase> home_use_case,
    std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase> ensure_ready_zero_use_case,
    std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase> manual_use_case,
    std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase> monitoring_use_case,
    std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort> hardware_connection_port)
    : home_use_case_(std::move(home_use_case)),
      ensure_ready_zero_use_case_(std::move(ensure_ready_zero_use_case)),
      manual_use_case_(std::move(manual_use_case)),
      monitoring_use_case_(std::move(monitoring_use_case)),
      hardware_connection_port_(std::move(hardware_connection_port)) {}

Shared::Types::Result<UseCases::Motion::Homing::HomeAxesResponse> TcpMotionFacade::Home(
    const UseCases::Motion::Homing::HomeAxesRequest& request) {
    if (!home_use_case_) {
        return Shared::Types::Result<UseCases::Motion::Homing::HomeAxesResponse>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "HomeAxesUseCase not available"));
    }
    return home_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Motion::Homing::EnsureAxesReadyZeroResponse> TcpMotionFacade::EnsureAxesReadyZero(
    const UseCases::Motion::Homing::EnsureAxesReadyZeroRequest& request) {
    if (!ensure_ready_zero_use_case_) {
        return Shared::Types::Result<UseCases::Motion::Homing::EnsureAxesReadyZeroResponse>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                "EnsureAxesReadyZeroUseCase not available"));
    }
    return ensure_ready_zero_use_case_->Execute(request);
}

Shared::Types::Result<bool> TcpMotionFacade::IsAxisHomed(Shared::Types::LogicalAxisId axis) const {
    if (!home_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "HomeAxesUseCase not available"));
    }
    return home_use_case_->IsAxisHomed(axis);
}

Shared::Types::Result<void> TcpMotionFacade::ExecutePointToPointMotion(
    const UseCases::Motion::Manual::ManualMotionCommand& command,
    bool invalidate_homing) {
    if (!manual_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "ManualMotionControlUseCase not available"));
    }
    return manual_use_case_->ExecutePointToPointMotion(command, invalidate_homing);
}

Shared::Types::Result<void> TcpMotionFacade::StartJog(
    Shared::Types::LogicalAxisId axis,
    int16 direction,
    float32 velocity) {
    if (!manual_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "ManualMotionControlUseCase not available"));
    }
    return manual_use_case_->StartJogMotion(axis, direction, velocity);
}

Shared::Types::Result<void> TcpMotionFacade::StopJog(Shared::Types::LogicalAxisId axis) {
    if (!manual_use_case_) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "ManualMotionControlUseCase not available"));
    }
    return manual_use_case_->StopJogMotion(axis);
}

Shared::Types::Result<Domain::Motion::Ports::MotionStatus> TcpMotionFacade::GetAxisMotionStatus(
    Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<Domain::Motion::Ports::MotionStatus>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetAxisMotionStatus(axis);
}

Shared::Types::Result<std::vector<Domain::Motion::Ports::MotionStatus>> TcpMotionFacade::GetAllAxesMotionStatus() const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetAllAxesMotionStatus();
}

Shared::Types::Result<Point2D> TcpMotionFacade::GetCurrentPosition() const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<Point2D>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetCurrentPosition();
}

Shared::Types::Result<Domain::Motion::Ports::CoordinateSystemStatus> TcpMotionFacade::GetCoordinateSystemStatus(
    int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<Domain::Motion::Ports::CoordinateSystemStatus>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetCoordinateSystemStatus(coord_sys);
}

Shared::Types::Result<uint32> TcpMotionFacade::GetInterpolationBufferSpace(int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<uint32>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetInterpolationBufferSpace(coord_sys);
}

Shared::Types::Result<uint32> TcpMotionFacade::GetLookAheadBufferSpace(int16 coord_sys) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<uint32>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->GetLookAheadBufferSpace(coord_sys);
}

Shared::Types::Result<bool> TcpMotionFacade::ReadLimitStatus(
    Shared::Types::LogicalAxisId axis,
    bool positive) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->ReadLimitStatus(axis, positive);
}

Shared::Types::Result<bool> TcpMotionFacade::ReadServoAlarmStatus(Shared::Types::LogicalAxisId axis) const {
    if (!monitoring_use_case_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED, "MotionMonitoringUseCase not available"));
    }
    return monitoring_use_case_->ReadServoAlarmStatus(axis);
}

bool TcpMotionFacade::HasHardwareConnectionPort() const {
    return static_cast<bool>(hardware_connection_port_);
}

Domain::Machine::Ports::HardwareConnectionInfo TcpMotionFacade::GetHardwareConnectionInfo() const {
    if (!hardware_connection_port_) {
        return {};
    }
    return hardware_connection_port_->GetConnectionInfo();
}

Domain::Machine::Ports::HeartbeatStatus TcpMotionFacade::GetHeartbeatStatus() const {
    if (!hardware_connection_port_) {
        return {};
    }
    return hardware_connection_port_->GetHeartbeatStatus();
}

bool TcpMotionFacade::IsHardwareReadyForMotion() const {
    if (!hardware_connection_port_) {
        return true;
    }

    const auto connection_info = hardware_connection_port_->GetConnectionInfo();
    const auto heartbeat_status = hardware_connection_port_->GetHeartbeatStatus();
    return connection_info.IsConnected() && !heartbeat_status.is_degraded;
}

}  // namespace Siligen::Application::Facades::Tcp
