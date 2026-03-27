#pragma once

#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::MotionPlanning::Contracts {

struct MotionPlanningRequest {
    Siligen::ProcessPath::Contracts::ProcessPath process_path{};
    TimePlanningConfig config{};
};

}  // namespace Siligen::MotionPlanning::Contracts
