#pragma once

#include "domain/trajectory/value-objects/ProcessPath.h"
#include "domain/trajectory/value-objects/MotionConfig.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::DomainServices::VelocityProfileService;

class MotionPlanner {
   public:
    explicit MotionPlanner(std::shared_ptr<VelocityProfileService> velocity_service = nullptr);
    ~MotionPlanner() = default;

    MotionTrajectory Plan(const ProcessPath& path, const MotionConfig& config) const;

   private:
    std::shared_ptr<VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Domain::Motion::DomainServices
