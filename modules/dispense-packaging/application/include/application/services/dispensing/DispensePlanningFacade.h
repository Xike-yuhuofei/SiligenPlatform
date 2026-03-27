#pragma once

#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"

#include <memory>

namespace Siligen::Application::Services::Dispensing {

class DispensePlanningFacade {
   public:
    explicit DispensePlanningFacade(
        std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort> path_source,
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_profile_service = nullptr);

    Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::DomainServices::DispensingPlan> Plan(
        const Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest& request) const noexcept;

    std::vector<Siligen::TrajectoryPoint> BuildPreviewPoints(
        const Siligen::Domain::Dispensing::DomainServices::DispensingPlan& plan,
        Siligen::Shared::Types::float32 spacing_mm,
        size_t max_points) const;

   private:
    std::shared_ptr<Siligen::Domain::Dispensing::DomainServices::DispensingPlanner> planner_;
};

}  // namespace Siligen::Application::Services::Dispensing
