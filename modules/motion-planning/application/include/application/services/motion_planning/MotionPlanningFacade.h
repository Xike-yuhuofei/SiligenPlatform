#pragma once

#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

#include <memory>

namespace Siligen::Domain::Motion::Ports {
class IVelocityProfilePort;
}

namespace Siligen::Application::Services::MotionPlanning {

class MotionPlanningFacade {
   public:
    explicit MotionPlanningFacade(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port = nullptr);

    Siligen::MotionPlanning::Contracts::MotionPlan Plan(
        const Siligen::ProcessPath::Contracts::ProcessPath& path,
        const Siligen::MotionPlanning::Contracts::TimePlanningConfig& config) const;

   private:
    std::shared_ptr<Siligen::Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port_;
};

}  // namespace Siligen::Application::Services::MotionPlanning
