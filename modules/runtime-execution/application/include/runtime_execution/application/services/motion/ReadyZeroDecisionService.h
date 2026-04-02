#pragma once

#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/AxisTypes.h"

#include <string>

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;

enum class ReadyZeroSupervisorState {
    BLOCKED,
    HOMING_IN_PROGRESS,
    NOT_HOMED,
    HOMED_AT_ZERO,
    HOMED_AWAY_FROM_ZERO,
};

enum class ReadyZeroPlannedAction {
    REJECT,
    NOOP,
    HOME,
    GO_HOME,
};

struct ReadyZeroDecision {
    LogicalAxisId axis = LogicalAxisId::INVALID;
    ReadyZeroSupervisorState supervisor_state = ReadyZeroSupervisorState::NOT_HOMED;
    ReadyZeroPlannedAction planned_action = ReadyZeroPlannedAction::HOME;
    std::string reason_code;
    std::string message;
    float32 position_mm = 0.0f;
    bool homed = false;
    bool at_zero = false;
    bool home_signal = false;
};

class ReadyZeroDecisionService final {
   public:
    ReadyZeroDecision EvaluateAxis(LogicalAxisId axis,
                                   const MotionStatus& status,
                                   float32 zero_tolerance_mm,
                                   float32 settled_velocity_threshold_mm_s) const;

    static const char* ToString(ReadyZeroSupervisorState state);
    static const char* ToString(ReadyZeroPlannedAction action);

   private:
    static float32 ExtractAxisPositionMm(LogicalAxisId axis, const MotionStatus& status);
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
