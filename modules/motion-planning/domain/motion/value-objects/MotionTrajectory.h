#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "domain/motion/value-objects/MotionPlanningReport.h"
#include "process_path/contracts/ProcessPath.h"

#include <vector>

namespace Siligen::Domain::Motion::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Point3D;
using Siligen::ProcessPath::Contracts::ProcessTag;

struct MotionTrajectoryPoint {
    float32 t = 0.0f;
    Point3D position{};
    Point3D velocity{};
    ProcessTag process_tag = ProcessTag::Normal;
    // Only indicates the path region where dispensing stays enabled; it is not a discrete trigger marker.
    bool dispense_on = false;
    float32 flow_rate = 0.0f;
};

struct MotionTrajectory {
    std::vector<MotionTrajectoryPoint> points;
    float32 total_time = 0.0f;
    float32 total_length = 0.0f;
    MotionPlanningReport planning_report{};
};

}  // namespace Siligen::Domain::Motion::ValueObjects
