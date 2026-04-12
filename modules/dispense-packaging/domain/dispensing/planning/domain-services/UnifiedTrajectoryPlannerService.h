#pragma once

#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "../../../../motion-planning/domain/motion/ports/IVelocityProfilePort.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/NormalizationConfig.h"
#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/TrajectoryShaperConfig.h"
#include "shared/types/Result.h"

#include <memory>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

struct UnifiedTrajectoryPlanRequest {
    Siligen::ProcessPath::Contracts::NormalizationConfig normalization;
    Siligen::ProcessPath::Contracts::ProcessConfig process;
    Siligen::ProcessPath::Contracts::TrajectoryShaperConfig shaping;
    Siligen::MotionPlanning::Contracts::TimePlanningConfig motion;
    bool generate_motion_trajectory = true;
};

struct UnifiedTrajectoryPlanResult {
    Siligen::ProcessPath::Contracts::NormalizedPath normalized;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    Siligen::ProcessPath::Contracts::ProcessPath shaped_path;
    Siligen::MotionPlanning::Contracts::MotionTrajectory motion_trajectory;
};

class UnifiedTrajectoryPlannerService {
   public:
    explicit UnifiedTrajectoryPlannerService(
        std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port = nullptr);
    ~UnifiedTrajectoryPlannerService() = default;

    Siligen::Shared::Types::Result<UnifiedTrajectoryPlanResult> Plan(
        const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
        const UnifiedTrajectoryPlanRequest& request) const;

   private:
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port_;
    Siligen::Application::Services::ProcessPath::ProcessPathFacade process_path_facade_{};
    Siligen::Application::Services::MotionPlanning::MotionPlanningFacade motion_planning_facade_{};
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


