#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::InterpolationType;

class InterpolationCommandValidator {
public:
    Result<void> ValidateInterpolationData(const InterpolationData& data) const noexcept;
    Result<void> ValidateVelocityOverride(float32 override_percent) const noexcept;
    Result<void> ValidateSCurveJerk(float32 jerk) const noexcept;
    Result<void> ValidateCoordinateSystemMask(uint32 coord_sys_mask) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices
