#pragma once

#include "siligen/shared/basic_types.h"

#include <string>
#include <vector>

namespace Siligen::Motion::Model {

enum class MotionCommandKind {
    kJog,
    kMoveTo,
    kHome,
    kStop,
    kEmergencyStop,
    kExecuteTrajectory,
};

enum class MotionLifecycleState {
    kIdle,
    kPrepared,
    kRunning,
    kPaused,
    kCompleted,
    kFaulted,
};

struct AxisTarget {
    Siligen::SharedKernel::LogicalAxisId axis = Siligen::SharedKernel::LogicalAxisId::X;
    double position_mm = 0.0;
    double velocity_mm_s = 0.0;
};

struct MotionCommand {
    MotionCommandKind kind = MotionCommandKind::kMoveTo;
    std::vector<AxisTarget> targets;
    bool relative = false;
    std::string correlation_id;
};

struct MotionState {
    MotionLifecycleState lifecycle = MotionLifecycleState::kIdle;
    bool estop_active = false;
    bool homed = false;
    std::string fault_message;
};

}  // namespace Siligen::Motion::Model
