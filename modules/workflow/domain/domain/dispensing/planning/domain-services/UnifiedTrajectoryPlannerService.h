#pragma once

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/motion/domain-services/VelocityProfileService.h"

#include <memory>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

struct UnifiedTrajectoryPlanRequest {
    Domain::Trajectory::DomainServices::NormalizationConfig normalization;
    Siligen::ProcessPath::Contracts::ProcessConfig process;
    Domain::Trajectory::DomainServices::TrajectoryShaperConfig shaping;
    Siligen::MotionPlanning::Contracts::TimePlanningConfig motion;
    bool generate_motion_trajectory = true;
};

struct UnifiedTrajectoryPlanResult {
    Domain::Trajectory::DomainServices::NormalizedPath normalized;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath shaped_path;
    Siligen::MotionPlanning::Contracts::MotionTrajectory motion_trajectory;
};

class UnifiedTrajectoryPlannerService {
   public:
    explicit UnifiedTrajectoryPlannerService(
        std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_service = nullptr);
    ~UnifiedTrajectoryPlannerService() = default;

    UnifiedTrajectoryPlanResult Plan(const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
                                     const UnifiedTrajectoryPlanRequest& request) const;

   private:
    std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_service_;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


