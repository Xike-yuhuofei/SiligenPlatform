#include "application/services/motion_planning/InterpolationProgramFacade.h"

#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"

namespace Siligen::Application::Services::MotionPlanning {

Siligen::Shared::Types::Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::InterpolationData>>
InterpolationProgramFacade::BuildProgram(const Siligen::ProcessPath::Contracts::ProcessPath& path,
                                         const Siligen::MotionPlanning::Contracts::MotionTrajectory& trajectory,
                                         Siligen::Shared::Types::float32 acceleration) const noexcept {
    Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner planner;
    return planner.BuildProgram(path, trajectory, acceleration);
}

}  // namespace Siligen::Application::Services::MotionPlanning
