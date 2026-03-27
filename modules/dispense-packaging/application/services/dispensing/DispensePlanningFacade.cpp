#include "application/services/dispensing/DispensePlanningFacade.h"

namespace Siligen::Application::Services::Dispensing {

DispensePlanningFacade::DispensePlanningFacade(
    std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source,
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_profile_service)
    : planner_(std::make_shared<Siligen::Domain::Dispensing::DomainServices::DispensingPlanner>(
          std::move(path_source),
          std::move(velocity_profile_service))) {}

Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::DomainServices::DispensingPlan>
DispensePlanningFacade::Plan(
    const Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest& request) const noexcept {
    return planner_->Plan(request);
}

std::vector<Siligen::TrajectoryPoint> DispensePlanningFacade::BuildPreviewPoints(
    const Siligen::Domain::Dispensing::DomainServices::DispensingPlan& plan,
    Siligen::Shared::Types::float32 spacing_mm,
    size_t max_points) const {
    return planner_->BuildPreviewPoints(plan, spacing_mm, max_points);
}

}  // namespace Siligen::Application::Services::Dispensing
