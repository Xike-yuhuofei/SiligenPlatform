#include "application/services/motion_planning/MotionPlanningFacade.h"

#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

namespace Siligen::Application::Services::MotionPlanning {

MotionPlanningFacade::MotionPlanningFacade(
    std::shared_ptr<Siligen::Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port)
    : velocity_profile_port_(std::move(velocity_profile_port)) {}

Siligen::MotionPlanning::Contracts::MotionPlan MotionPlanningFacade::Plan(
    const Siligen::ProcessPath::Contracts::ProcessPath& path,
    const Siligen::MotionPlanning::Contracts::TimePlanningConfig& config) const {
    auto velocity_service =
        velocity_profile_port_
            ? std::make_shared<Siligen::Domain::Motion::DomainServices::VelocityProfileService>(velocity_profile_port_)
            : nullptr;
    Siligen::Domain::Motion::DomainServices::MotionPlanner planner(std::move(velocity_service));
    return planner.Plan(path, config);
}

}  // namespace Siligen::Application::Services::MotionPlanning
