#include "application/services/motion_planning/MotionPlanningFacade.h"

#include "domain/motion/domain-services/MotionPlanner.h"

namespace Siligen::Application::Services::MotionPlanning {

MotionPlanningFacade::MotionPlanningFacade(
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_service)
    : velocity_service_(std::move(velocity_service)) {}

Siligen::MotionPlanning::Contracts::MotionPlan MotionPlanningFacade::Plan(
    const Siligen::ProcessPath::Contracts::ProcessPath& path,
    const Siligen::MotionPlanning::Contracts::TimePlanningConfig& config) const {
    Siligen::Domain::Motion::DomainServices::MotionPlanner planner(velocity_service_);
    return planner.Plan(path, config);
}

}  // namespace Siligen::Application::Services::MotionPlanning
