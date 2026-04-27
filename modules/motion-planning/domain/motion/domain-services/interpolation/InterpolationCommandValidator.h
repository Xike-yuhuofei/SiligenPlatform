#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;

class InterpolationCommandValidator {
public:
    static Result<void> ValidateInterpolationData(const InterpolationData& data) noexcept;
    Result<void> ValidateVelocityOverride(float32 override_percent) const noexcept;
    Result<void> ValidateSCurveJerk(float32 jerk) const noexcept;
    Result<void> ValidateCoordinateSystemMask(uint32 coord_sys_mask) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices
