#pragma once

#include "motion_planning/contracts/MotionPlanningReport.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::MotionPlanning::Contracts {

using Siligen::Point3D;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::Shared::Types::float32;

struct MotionTrajectoryPoint {
    float32 t = 0.0f;
    Point3D position{};
    Point3D velocity{};
    ProcessTag process_tag = ProcessTag::Normal;
    bool dispense_on = false;
    float32 flow_rate = 0.0f;
};

struct MotionTrajectory {
    std::vector<MotionTrajectoryPoint> points;
    float32 total_time = 0.0f;
    float32 total_length = 0.0f;
    MotionPlanningReport planning_report{};
};

}  // namespace Siligen::MotionPlanning::Contracts

namespace Siligen::Domain::Motion::ValueObjects {

using MotionTrajectoryPoint = Siligen::MotionPlanning::Contracts::MotionTrajectoryPoint;
using MotionTrajectory = Siligen::MotionPlanning::Contracts::MotionTrajectory;

}  // namespace Siligen::Domain::Motion::ValueObjects
