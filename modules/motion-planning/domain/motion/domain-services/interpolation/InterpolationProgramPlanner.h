#pragma once

#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/trajectory/value-objects/ProcessPath.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;

class InterpolationProgramPlanner {
public:
    Result<std::vector<InterpolationData>> BuildProgram(const ProcessPath& path,
                                                        const MotionTrajectory& trajectory,
                                                        float32 acceleration) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices
