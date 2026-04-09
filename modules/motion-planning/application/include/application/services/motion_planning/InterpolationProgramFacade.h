#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "process_path/contracts/ProcessPath.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Application::Services::MotionPlanning {

class InterpolationProgramFacade {
   public:
    Siligen::Shared::Types::Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::InterpolationData>>
    BuildProgram(const Siligen::ProcessPath::Contracts::ProcessPath& path,
                 const Siligen::MotionPlanning::Contracts::MotionTrajectory& trajectory,
                 Siligen::Shared::Types::float32 acceleration) const noexcept;
};

}  // namespace Siligen::Application::Services::MotionPlanning
