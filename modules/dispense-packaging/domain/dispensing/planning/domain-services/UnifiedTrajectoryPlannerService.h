#pragma once

#include "domain/motion/value-objects/TimePlanningConfig.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/domain-services/VelocityProfileService.h"
#include "process_path/contracts/NormalizationConfig.h"
#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/TrajectoryShaperConfig.h"

#include <memory>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

struct UnifiedTrajectoryPlanRequest {
    Siligen::ProcessPath::Contracts::NormalizationConfig normalization;
    Siligen::ProcessPath::Contracts::ProcessConfig process;
    Siligen::ProcessPath::Contracts::TrajectoryShaperConfig shaping;
    Domain::Motion::ValueObjects::TimePlanningConfig motion;
    bool generate_motion_trajectory = true;
};

struct UnifiedTrajectoryPlanResult {
    Siligen::ProcessPath::Contracts::NormalizedPath normalized;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath shaped_path;
    Domain::Motion::ValueObjects::MotionTrajectory motion_trajectory;
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


