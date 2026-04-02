#pragma once

#include "domain/motion/ports/IHomingPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "runtime_execution/application/services/motion/JogController.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <memory>

namespace Siligen::Application::UseCases::Motion::Manual {

struct ManualMotionCommand {
    Siligen::Shared::Types::LogicalAxisId axis = Siligen::Shared::Types::LogicalAxisId::X;
    float32 position = 0.0f;
    float32 velocity = 0.0f;
    float32 acceleration = 0.0f;
    bool relative_move = false;
};

struct ManualHandwheelCommand {
    enum class Action {
        ENTER_MODE,
        EXIT_MODE,
        SELECT_AXIS,
        SET_MULTIPLIER
    } action;

    Siligen::Shared::Types::LogicalAxisId axis = Siligen::Shared::Types::LogicalAxisId::X;
    int16 multiplier = 1;
    bool enabled = false;
};

class ManualMotionControlUseCase {
   public:
    using MotionStatusCallback =
        std::function<void(Siligen::Shared::Types::LogicalAxisId, const Domain::Motion::Ports::MotionStatus&)>;
    using IOStatusCallback =
        std::function<void(const Siligen::RuntimeExecution::Contracts::Motion::IOStatus&)>;

    ManualMotionControlUseCase(
        std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
        std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::JogController> jog_controller,
        std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port = nullptr);

    ~ManualMotionControlUseCase() = default;

    Result<void> ExecutePointToPointMotion(const ManualMotionCommand& command, bool invalidate_homing = false);
    Result<void> StartJogMotion(Siligen::Shared::Types::LogicalAxisId axis, int16 direction, float32 velocity);
    Result<void> StopJogMotion(Siligen::Shared::Types::LogicalAxisId axis);
    Result<void> StartJogMotionStep(Siligen::Shared::Types::LogicalAxisId axis,
                                    int16 direction,
                                    float32 distance,
                                    float32 velocity);

    Result<void> ExecuteHandwheelCommand(const ManualHandwheelCommand& command);
    Result<void> SelectHandwheelAxis(Siligen::Shared::Types::LogicalAxisId axis);
    Result<void> SetHandwheelMultiplier(int16 multiplier);

   private:
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
    std::shared_ptr<Siligen::RuntimeExecution::Application::Services::Motion::JogController> jog_controller_;
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port_;

    void InvalidateHomingState(Siligen::Shared::Types::LogicalAxisId axis_id);
};

}  // namespace Siligen::Application::UseCases::Motion::Manual
