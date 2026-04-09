#pragma once

#include "shared/types/AxisStatus.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::RuntimeExecution::Contracts::Motion {

using Siligen::Shared::Types::AxisStatus;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

class MotionStatusService {
   public:
    virtual ~MotionStatusService() = default;

    virtual Result<Point2D> GetCurrentPosition() const = 0;
    virtual Result<AxisStatus> GetAxisStatus(LogicalAxisId axis_id) const = 0;
    virtual Result<std::vector<AxisStatus>> GetAllAxisStatus() const = 0;

    virtual Result<bool> IsMoving() const = 0;
    virtual Result<bool> HasError() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::DomainServices {

using MotionStatusService = Siligen::RuntimeExecution::Contracts::Motion::MotionStatusService;

}  // namespace Siligen::Domain::Motion::DomainServices
