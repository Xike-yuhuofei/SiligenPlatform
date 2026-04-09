#pragma once

#include "shared/types/Point2D.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::Motion {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;

class MotionValidationService {
   public:
    virtual ~MotionValidationService() = default;

    virtual Result<void> ValidatePosition(const Point2D& position) const = 0;
    virtual Result<void> ValidateSpeed(float speed) const = 0;
    virtual Result<bool> IsPositionErrorExceeded(const Point2D& target, const Point2D& actual) const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::DomainServices {

using MotionValidationService = Siligen::RuntimeExecution::Contracts::Motion::MotionValidationService;

}  // namespace Siligen::Domain::Motion::DomainServices
