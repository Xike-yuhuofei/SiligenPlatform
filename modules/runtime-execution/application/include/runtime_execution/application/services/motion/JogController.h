#pragma once

#include "domain/motion/ports/IJogControlPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Domain::Motion::Ports::IJogControlPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

enum class JogState : uint8_t {
    Idle = 0,
    Jogging = 1,
    Stopping = 2,
    Fault = 3
};

class JogController {
   public:
    explicit JogController(std::shared_ptr<IJogControlPort> jog_port,
                           std::shared_ptr<IMotionStatePort> state_port) noexcept;

    Result<void> StartContinuousJog(LogicalAxisId axis, int16_t direction, float velocity) noexcept;
    Result<void> StartStepJog(LogicalAxisId axis, int16_t direction, float distance, float velocity) noexcept;
    Result<void> StopJog(LogicalAxisId axis) noexcept;
    Result<JogState> GetJogState(LogicalAxisId axis) const noexcept;

   private:
    std::shared_ptr<IJogControlPort> jog_port_;
    std::shared_ptr<IMotionStatePort> state_port_;

    Result<void> CheckSafetyInterlocks(LogicalAxisId axis, int16_t direction) const noexcept;
    Result<void> ValidateJogParameters(LogicalAxisId axis, float velocity) const noexcept;
    Result<void> ValidateDistance(float distance) const noexcept;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
