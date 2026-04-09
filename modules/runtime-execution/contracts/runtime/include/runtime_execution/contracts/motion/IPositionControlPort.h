#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <vector>

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

namespace Siligen::RuntimeExecution::Contracts::Motion {

struct MotionCommand {
    LogicalAxisId axis = LogicalAxisId::INVALID;
    float32 position = 0.0f;
    float32 velocity = 0.0f;
    bool relative = false;
};

class IPositionControlPort {
   public:
    virtual ~IPositionControlPort() = default;

    virtual Result<void> MoveToPosition(const Point2D& position, float32 velocity) = 0;
    virtual Result<void> MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) = 0;
    virtual Result<void> RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) = 0;
    virtual Result<void> SynchronizedMove(const std::vector<MotionCommand>& commands) = 0;

    virtual Result<void> StopAxis(LogicalAxisId axis, bool immediate = false) = 0;
    virtual Result<void> StopAllAxes(bool immediate = false) = 0;
    virtual Result<void> EmergencyStop() = 0;
    virtual Result<void> RecoverFromEmergencyStop() = 0;
    virtual Result<void> WaitForMotionComplete(LogicalAxisId axis, int32 timeout_ms = 60000) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IPositionControlPort = Siligen::RuntimeExecution::Contracts::Motion::IPositionControlPort;
using MotionCommand = Siligen::RuntimeExecution::Contracts::Motion::MotionCommand;

}  // namespace Siligen::Domain::Motion::Ports
