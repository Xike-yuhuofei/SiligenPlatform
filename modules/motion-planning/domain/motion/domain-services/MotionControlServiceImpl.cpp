#include "MotionControlServiceImpl.h"
#include "shared/util/PortCheck.h"

namespace Siligen::Domain::Motion::DomainServices {

namespace {

Result<void> EnsurePositionControlPort(
    const std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort>& port,
    const char* operation) {
    return Shared::Util::EnsurePort(
        port,
        "Position control port not available",
        operation,
        Shared::Types::ErrorCode::INVALID_STATE);
}

}  // namespace

Result<void> MotionControlServiceImpl::MoveToPosition(const Point2D& position, float speed) {
    auto port_check = EnsurePositionControlPort(position_control_port_, "MotionControlServiceImpl::MoveToPosition");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->MoveToPosition(position, static_cast<Shared::Types::float32>(speed));
}

Result<void> MotionControlServiceImpl::MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed) {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "MotionControlServiceImpl::MoveAxisToPosition");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->MoveAxisToPosition(axis_id,
                                                      static_cast<Shared::Types::float32>(position),
                                                      static_cast<Shared::Types::float32>(speed));
}

Result<void> MotionControlServiceImpl::MoveRelative(const Point2D& offset, float speed) {
    auto port_check = EnsurePositionControlPort(position_control_port_, "MotionControlServiceImpl::MoveRelative");
    if (port_check.IsError()) {
        return port_check;
    }

    std::vector<Siligen::Domain::Motion::Ports::MotionCommand> commands;
    commands.push_back({Shared::Types::LogicalAxisId::X,
                        offset.x,
                        static_cast<Shared::Types::float32>(speed),
                        true});
    commands.push_back({Shared::Types::LogicalAxisId::Y,
                        offset.y,
                        static_cast<Shared::Types::float32>(speed),
                        true});
    return position_control_port_->SynchronizedMove(commands);
}

Result<void> MotionControlServiceImpl::StopAllAxes() {
    auto port_check = EnsurePositionControlPort(position_control_port_, "MotionControlServiceImpl::StopAllAxes");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->StopAllAxes(false);
}

Result<void> MotionControlServiceImpl::EmergencyStop() {
    auto port_check = EnsurePositionControlPort(position_control_port_, "MotionControlServiceImpl::EmergencyStop");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->EmergencyStop();
}

}  // namespace Siligen::Domain::Motion::DomainServices
