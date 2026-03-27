#pragma once

#include "domain/trajectory/value-objects/ProcessPath.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/value-objects/TimePlanningConfig.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
using Siligen::Domain::Motion::DomainServices::VelocityProfileService;

class MotionPlanner {
   public:
    explicit MotionPlanner(std::shared_ptr<VelocityProfileService> velocity_service = nullptr);
    ~MotionPlanner() = default;

    MotionTrajectory Plan(const ProcessPath& path, const TimePlanningConfig& config) const;

   private:
    std::shared_ptr<VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Domain::Motion::DomainServices
