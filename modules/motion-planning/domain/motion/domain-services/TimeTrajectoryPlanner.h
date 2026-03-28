#pragma once

#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/value-objects/SemanticPath.h"
#include "domain/motion/value-objects/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::SemanticPath;
using Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;

class TimeTrajectoryPlanner {
   public:
    TimeTrajectoryPlanner() = default;
    ~TimeTrajectoryPlanner() = default;

    MotionTrajectory Plan(const SemanticPath& path, const TimePlanningConfig& config) const;
    MotionTrajectory Plan(const ProcessPath& path, const TimePlanningConfig& config) const;
};

}  // namespace Siligen::Domain::Motion::DomainServices
