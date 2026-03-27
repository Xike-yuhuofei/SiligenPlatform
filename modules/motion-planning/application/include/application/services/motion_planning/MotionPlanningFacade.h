#pragma once

#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

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
    Siligen::MotionPlanning::Contracts::MotionPlan Plan(
        const Siligen::ProcessPath::Contracts::ProcessPath& path,
        const Siligen::MotionPlanning::Contracts::TimePlanningConfig& config) const;

   private:
    std::shared_ptr<Siligen::Domain::Motion::DomainServices::VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Application::Services::MotionPlanning
