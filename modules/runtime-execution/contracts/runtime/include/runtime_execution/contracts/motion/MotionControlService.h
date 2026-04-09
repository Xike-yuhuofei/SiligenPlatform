#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::Motion {

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

class MotionControlService {
   public:
    virtual ~MotionControlService() = default;

    virtual Result<void> MoveToPosition(const Point2D& position, float speed = -1.0f) = 0;
    virtual Result<void> MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed = -1.0f) = 0;
    virtual Result<void> MoveRelative(const Point2D& offset, float speed = -1.0f) = 0;

    virtual Result<void> StopAllAxes() = 0;
    virtual Result<void> EmergencyStop() = 0;
    virtual Result<void> RecoverFromEmergencyStop() = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::DomainServices {

using MotionControlService = Siligen::RuntimeExecution::Contracts::Motion::MotionControlService;

}  // namespace Siligen::Domain::Motion::DomainServices
