#include "MotionSafetyUseCase.h"

#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Safety {

MotionSafetyUseCase::MotionSafetyUseCase(
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port)
    : position_control_port_(std::move(position_control_port)) {}

Result<void> MotionSafetyUseCase::EmergencyStop() {
    if (position_control_port_) {
        return position_control_port_->EmergencyStop();
    }

    return Result<void>::Failure(Error(
        ErrorCode::INVALID_STATE,
        "No emergency stop path available",
        "MotionSafetyUseCase::EmergencyStop"
    ));
}

Result<void> MotionSafetyUseCase::StopAllAxes(bool immediate) {
    if (position_control_port_) {
        return position_control_port_->StopAllAxes(immediate);
    }

    return Result<void>::Failure(Error(
        ErrorCode::INVALID_STATE,
        "Position control port not available",
        "MotionSafetyUseCase::StopAllAxes"
    ));
}

}  // namespace Siligen::Application::UseCases::Motion::Safety


