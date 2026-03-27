#include "application/services/motion_planning/MotionPlanningFacade.h"

#include "domain/trajectory/domain-services/MotionPlanner.h"

namespace Siligen::Application::Services::MotionPlanning {

MotionPlanningFacade::MotionPlanningFacade(
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_service)
    : velocity_service_(std::move(velocity_service)) {}

Siligen::Domain::Motion::ValueObjects::MotionTrajectory MotionPlanningFacade::Plan(
    const Siligen::Domain::Trajectory::ValueObjects::ProcessPath& path,
    const Siligen::Domain::Trajectory::ValueObjects::MotionConfig& config) const {
    Siligen::Domain::Trajectory::DomainServices::MotionPlanner planner(velocity_service_);
    return planner.Plan(path, config);
}

}  // namespace Siligen::Application::Services::MotionPlanning
