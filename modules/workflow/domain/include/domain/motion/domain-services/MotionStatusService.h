#pragma once

#if __has_include("runtime_execution/contracts/motion/MotionStatusService.h")
#include "runtime_execution/contracts/motion/MotionStatusService.h"
#else
#include "shared/types/AxisStatus.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <vector>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::AxisStatus;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::LogicalAxisId;

class MotionStatusService {
public:
    virtual ~MotionStatusService() = default;

    virtual Result<Point2D> GetCurrentPosition() const = 0;
    virtual Result<AxisStatus> GetAxisStatus(LogicalAxisId axis_id) const = 0;
    virtual Result<std::vector<AxisStatus>> GetAllAxisStatus() const = 0;

    virtual Result<bool> IsMoving() const = 0;
    virtual Result<bool> HasError() const = 0;
};

}  // namespace Siligen::Domain::Motion::DomainServices
#endif

