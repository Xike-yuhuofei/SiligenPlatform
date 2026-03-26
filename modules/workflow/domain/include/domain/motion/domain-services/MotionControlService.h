#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <cstdint>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::LogicalAxisId;

class MotionControlService {
public:
    virtual ~MotionControlService() = default;

    virtual Result<void> MoveToPosition(const Point2D& position, float speed = -1.0f) = 0;
    virtual Result<void> MoveAxisToPosition(LogicalAxisId axis_id, float position, float speed = -1.0f) = 0;
    virtual Result<void> MoveRelative(const Point2D& offset, float speed = -1.0f) = 0;

    virtual Result<void> StopAllAxes() = 0;
    virtual Result<void> EmergencyStop() = 0;
};

}  // namespace Siligen::Domain::Motion::DomainServices

