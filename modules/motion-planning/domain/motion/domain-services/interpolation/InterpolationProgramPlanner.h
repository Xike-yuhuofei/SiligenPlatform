#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "process_path/contracts/ProcessPath.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;

class InterpolationProgramPlanner {
public:
    Result<std::vector<InterpolationData>> BuildProgram(const ProcessPath& path,
                                                        const MotionTrajectory& trajectory,
                                                        float32 acceleration) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices
