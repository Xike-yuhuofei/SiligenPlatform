#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"

#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Manual {

ManualMotionControlUseCase::ManualMotionControlUseCase(
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
    std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::JogController> jog_controller,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port)
    : position_control_port_(std::move(position_control_port)),
      jog_controller_(std::move(jog_controller)),
      homing_port_(std::move(homing_port)) {}

void ManualMotionControlUseCase::InvalidateHomingState(LogicalAxisId axis_id) {
    if (!homing_port_) {
        return;
    }
    (void)homing_port_->ResetHomingState(axis_id);
}

Result<void> ManualMotionControlUseCase::ExecutePointToPointMotion(const ManualMotionCommand& command,
                                                                   bool invalidate_homing) {
    if (!position_control_port_) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Position control port not available",
            "ManualMotionControlUseCase::ExecutePointToPointMotion"));
    }

    auto axis_id = command.axis;
    if (!IsValid(axis_id)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Invalid axis number",
            "ManualMotionControlUseCase::ExecutePointToPointMotion"));
    }

    Result<void> motion_result = command.relative_move
        ? position_control_port_->RelativeMove(axis_id, command.position, command.velocity)
        : position_control_port_->MoveAxisToPosition(axis_id, command.position, command.velocity);

    if (motion_result.IsSuccess() && invalidate_homing) {
        InvalidateHomingState(axis_id);
    }

    return motion_result;
}

Result<void> ManualMotionControlUseCase::StartJogMotion(LogicalAxisId axis_id, int16 direction, float32 velocity) {
    if (!jog_controller_) {
        return Result<void>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Jog controller not initialized",
            "ManualMotionControlUseCase::StartJogMotion"));
    }

    return jog_controller_->StartContinuousJog(axis_id, direction, velocity);
}

Result<void> ManualMotionControlUseCase::StartJogMotionStep(LogicalAxisId axis_id,
                                                            int16 direction,
                                                            float32 distance,
                                                            float32 velocity) {
    if (!jog_controller_) {
        return Result<void>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Jog controller not initialized",
            "ManualMotionControlUseCase::StartJogMotionStep"));
    }

    return jog_controller_->StartStepJog(axis_id, direction, distance, velocity);
}

Result<void> ManualMotionControlUseCase::StopJogMotion(LogicalAxisId axis_id) {
    if (!jog_controller_) {
        return Result<void>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Jog controller not initialized",
            "ManualMotionControlUseCase::StopJogMotion"));
    }

    return jog_controller_->StopJog(axis_id);
}

Result<void> ManualMotionControlUseCase::ExecuteHandwheelCommand(const ManualHandwheelCommand& command) {
    (void)command;
    return Result<void>::Success();
}

Result<void> ManualMotionControlUseCase::SelectHandwheelAxis(LogicalAxisId axis_id) {
    (void)axis_id;
    return Result<void>::Success();
}

Result<void> ManualMotionControlUseCase::SetHandwheelMultiplier(int16 multiplier) {
    (void)multiplier;
    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Motion::Manual
