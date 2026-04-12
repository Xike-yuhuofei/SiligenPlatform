#pragma once

#include "motion_planning/contracts/MotionTypes.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

using Siligen::Domain::Motion::ValueObjects::HomingStage;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

namespace Siligen::RuntimeExecution::Contracts::Motion {

enum class HomingState {
    NOT_HOMED,
    HOMING,
    HOMED,
    FAILED
};

struct HomingStatus {
    LogicalAxisId axis = LogicalAxisId::X;
    HomingState state = HomingState::NOT_HOMED;
    HomingStage stage{};
    float32 current_position = 0.0f;
    int32 error_code = 0;
    bool is_searching = false;
};

class IHomingPort {
   public:
    virtual ~IHomingPort() = default;

    virtual Result<void> HomeAxis(LogicalAxisId axis) = 0;
    virtual Result<void> StopHoming(LogicalAxisId axis) = 0;
    virtual Result<void> ResetHomingState(LogicalAxisId axis) = 0;
    virtual Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsAxisHomed(LogicalAxisId axis) const = 0;
    virtual Result<bool> IsHomingInProgress(LogicalAxisId axis) const = 0;
    virtual Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms = 80000) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using HomingState = Siligen::RuntimeExecution::Contracts::Motion::HomingState;
using HomingStatus = Siligen::RuntimeExecution::Contracts::Motion::HomingStatus;
using IHomingPort = Siligen::RuntimeExecution::Contracts::Motion::IHomingPort;

}  // namespace Siligen::Domain::Motion::Ports
