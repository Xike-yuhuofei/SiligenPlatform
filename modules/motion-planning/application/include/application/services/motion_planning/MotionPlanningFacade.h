#pragma once

#include "domain/trajectory/value-objects/MotionConfig.h"
#include "domain/trajectory/value-objects/ProcessPath.h"
#include "domain/motion/value-objects/MotionTrajectory.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {
class VelocityProfileService;
}

namespace Siligen::Application::Services::MotionPlanning {

class MotionPlanningFacade {
   public:
    explicit MotionPlanningFacade(
        std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_service = nullptr);

    // Canonical planner owner lives in domain/motion/domain-services/MotionPlanner.
    Siligen::Domain::Motion::ValueObjects::MotionTrajectory Plan(
        const Siligen::Domain::Trajectory::ValueObjects::ProcessPath& path,
        const Siligen::Domain::Trajectory::ValueObjects::MotionConfig& config) const;

   private:
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Application::Services::MotionPlanning
