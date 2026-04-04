#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "domain/motion/value-objects/SemanticPath.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::SemanticPath;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;

class TimeTrajectoryPlanner {
   public:
    TimeTrajectoryPlanner() = default;
    ~TimeTrajectoryPlanner() = default;

    MotionTrajectory Plan(const SemanticPath& path, const TimePlanningConfig& config) const;
    MotionTrajectory Plan(const ProcessPath& path, const TimePlanningConfig& config) const;
};

}  // namespace Siligen::Domain::Motion::DomainServices
