#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"

#include "shared/util/PortCheck.h"

#include <utility>
#include <vector>

namespace Siligen::RuntimeExecution::Application::Services::Motion {
namespace {

using PositionControlPort = Siligen::Domain::Motion::Ports::IPositionControlPort;

Siligen::Shared::Types::Result<void> EnsurePositionControlPort(
    const std::shared_ptr<PositionControlPort>& port,
    const char* operation) {
    return Siligen::Shared::Util::EnsurePort(
        port,
        "Position control port not available",
        operation,
        Siligen::Shared::Types::ErrorCode::INVALID_STATE);
}

}  // namespace

MotionControlServiceImpl::MotionControlServiceImpl(
    std::shared_ptr<Siligen::Domain::Motion::Ports::IPositionControlPort> position_control_port)
    : position_control_port_(std::move(position_control_port)) {}

Result<void> MotionControlServiceImpl::MoveToPosition(const Point2D& position, float speed) {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "RuntimeExecution::MotionControlServiceImpl::MoveToPosition");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->MoveToPosition(position, static_cast<Siligen::Shared::Types::float32>(speed));
}

Result<void> MotionControlServiceImpl::MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed) {
    auto port_check = EnsurePositionControlPort(
        position_control_port_,
        "RuntimeExecution::MotionControlServiceImpl::MoveAxisToPosition");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->MoveAxisToPosition(
        axis_id,
        static_cast<Siligen::Shared::Types::float32>(position),
        static_cast<Siligen::Shared::Types::float32>(speed));
}

Result<void> MotionControlServiceImpl::MoveRelative(const Point2D& offset, float speed) {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "RuntimeExecution::MotionControlServiceImpl::MoveRelative");
    if (port_check.IsError()) {
        return port_check;
    }

    std::vector<Siligen::Domain::Motion::Ports::MotionCommand> commands;
    commands.push_back({Siligen::Shared::Types::LogicalAxisId::X,
                        offset.x,
                        static_cast<Siligen::Shared::Types::float32>(speed),
                        true});
    commands.push_back({Siligen::Shared::Types::LogicalAxisId::Y,
                        offset.y,
                        static_cast<Siligen::Shared::Types::float32>(speed),
                        true});
    return position_control_port_->SynchronizedMove(commands);
}

Result<void> MotionControlServiceImpl::StopAllAxes() {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "RuntimeExecution::MotionControlServiceImpl::StopAllAxes");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->StopAllAxes(false);
}

Result<void> MotionControlServiceImpl::EmergencyStop() {
    auto port_check =
        EnsurePositionControlPort(position_control_port_, "RuntimeExecution::MotionControlServiceImpl::EmergencyStop");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->EmergencyStop();
}

Result<void> MotionControlServiceImpl::RecoverFromEmergencyStop() {
    auto port_check = EnsurePositionControlPort(
        position_control_port_,
        "RuntimeExecution::MotionControlServiceImpl::RecoverFromEmergencyStop");
    if (port_check.IsError()) {
        return port_check;
    }
    return position_control_port_->RecoverFromEmergencyStop();
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
