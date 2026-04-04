#pragma once

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "domain/trajectory/value-objects/Primitive.h"
#include "domain/trajectory/value-objects/ProcessConfig.h"
#include "domain/trajectory/value-objects/ProcessPath.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

#include <memory>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

struct UnifiedTrajectoryPlanRequest {
    Domain::Trajectory::DomainServices::NormalizationConfig normalization;
    Domain::Trajectory::ValueObjects::ProcessConfig process;
    Domain::Trajectory::DomainServices::TrajectoryShaperConfig shaping;
    Siligen::MotionPlanning::Contracts::TimePlanningConfig motion;
    bool generate_motion_trajectory = true;
};

struct UnifiedTrajectoryPlanResult {
    Domain::Trajectory::DomainServices::NormalizedPath normalized;
    Domain::Trajectory::ValueObjects::ProcessPath process_path;
    Domain::Trajectory::ValueObjects::ProcessPath shaped_path;
    Siligen::MotionPlanning::Contracts::MotionTrajectory motion_trajectory;
};

class UnifiedTrajectoryPlannerService {
   public:
    explicit UnifiedTrajectoryPlannerService(
        std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_service = nullptr);
    ~UnifiedTrajectoryPlannerService() = default;

    UnifiedTrajectoryPlanResult Plan(const std::vector<Domain::Trajectory::ValueObjects::Primitive>& primitives,
                                     const UnifiedTrajectoryPlanRequest& request) const;

   private:
    std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


