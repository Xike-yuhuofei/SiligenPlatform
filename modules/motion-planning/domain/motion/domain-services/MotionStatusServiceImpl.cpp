#include "MotionStatusServiceImpl.h"

namespace Siligen::Domain::Motion::DomainServices {

Result<Point2D> MotionStatusServiceImpl::GetCurrentPosition() const {
    if (!motion_state_port_) {
        return Result<Point2D>::Failure(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_STATE,
            "Motion state port not available",
            "MotionStatusServiceImpl::GetCurrentPosition"
        ));
    }
    return motion_state_port_->GetCurrentPosition();
}

namespace {

Siligen::Shared::Types::AxisState MapAxisState(Siligen::Domain::Motion::Ports::MotionState state) {
    using Siligen::Domain::Motion::Ports::MotionState;
    using Siligen::Shared::Types::AxisState;

    switch (state) {
        case MotionState::MOVING:
            return AxisState::MOVING;
        case MotionState::HOMING:
            return AxisState::HOMING;
        case MotionState::ESTOP:
            return AxisState::EMERGENCY_STOP;
        case MotionState::FAULT:
            return AxisState::FAULT;
        case MotionState::DISABLED:
            return AxisState::DISABLED;
        case MotionState::HOMED:
        case MotionState::STOPPED:
        case MotionState::IDLE:
        default:
            return AxisState::ENABLED;
    }
}

Siligen::Shared::Types::AxisStatus MapAxisStatus(const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    Siligen::Shared::Types::AxisStatus axis_status;
    axis_status.state = MapAxisState(status.state);
    axis_status.current_position = status.position.x;
    axis_status.target_position = 0.0f;
    axis_status.current_velocity = status.velocity;
    axis_status.is_homed = (status.state == Siligen::Domain::Motion::Ports::MotionState::HOMED);
    axis_status.has_error = status.has_error;
    axis_status.last_error_code = status.has_error
        ? Siligen::Shared::Types::ErrorCode::MOTION_ERROR
        : Siligen::Shared::Types::ErrorCode::SUCCESS;
    axis_status.UpdateTimestamp();
    return axis_status;
}

}  // namespace

Result<AxisStatus> MotionStatusServiceImpl::GetAxisStatus(LogicalAxisId axis_id) const {
    if (!motion_state_port_) {
        return Result<AxisStatus>::Failure(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_STATE,
            "Motion state port not available",
            "MotionStatusServiceImpl::GetAxisStatus"
        ));
    }

    auto status_result = motion_state_port_->GetAxisStatus(axis_id);
    if (status_result.IsError()) {
        return Result<AxisStatus>::Failure(status_result.GetError());
    }
    return Result<AxisStatus>::Success(MapAxisStatus(status_result.Value()));
}

Result<std::vector<AxisStatus>> MotionStatusServiceImpl::GetAllAxisStatus() const {
    if (!motion_state_port_) {
        return Result<std::vector<AxisStatus>>::Failure(Shared::Types::Error(
            Shared::Types::ErrorCode::INVALID_STATE,
            "Motion state port not available",
            "MotionStatusServiceImpl::GetAllAxisStatus"
        ));
    }

    auto status_result = motion_state_port_->GetAllAxesStatus();
    if (status_result.IsError()) {
        return Result<std::vector<AxisStatus>>::Failure(status_result.GetError());
    }

    std::vector<AxisStatus> mapped;
    mapped.reserve(status_result.Value().size());
    for (const auto& status : status_result.Value()) {
        mapped.push_back(MapAxisStatus(status));
    }

    return Result<std::vector<AxisStatus>>::Success(mapped);
}

Result<bool> MotionStatusServiceImpl::IsMoving() const {
    auto statuses = GetAllAxisStatus();
    if (statuses.IsError()) {
        return Result<bool>::Failure(statuses.GetError());
    }

    for (const auto& status : statuses.Value()) {
        if (status.IsMoving()) {
            return Result<bool>::Success(true);
        }
    }
    return Result<bool>::Success(false);
}

Result<bool> MotionStatusServiceImpl::HasError() const {
    auto statuses = GetAllAxisStatus();
    if (statuses.IsError()) {
        return Result<bool>::Failure(statuses.GetError());
    }

    for (const auto& status : statuses.Value()) {
        if (status.IsInError()) {
            return Result<bool>::Success(true);
        }
    }
    return Result<bool>::Success(false);
}

}  // namespace Siligen::Domain::Motion::DomainServices
