#pragma once

#include "domain/motion/domain-services/VelocityProfileService.h"
#include "motion_planning/contracts/MotionPlan.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::Domain::Motion::DomainServices::VelocityProfileService;

class MotionPlanner {
   public:
    explicit MotionPlanner(std::shared_ptr<VelocityProfileService> velocity_service = nullptr);
    ~MotionPlanner() = default;

    MotionPlan Plan(const ProcessPath& path, const TimePlanningConfig& config) const;

   private:
    std::shared_ptr<VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Domain::Motion::DomainServices
