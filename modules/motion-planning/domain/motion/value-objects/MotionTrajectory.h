#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "domain/motion/value-objects/MotionPlanningReport.h"
#include "domain/trajectory/value-objects/ProcessPath.h"

#include <vector>

namespace Siligen::Domain::Motion::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Point3D;
using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;

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

}  // namespace Siligen::Domain::Motion::ValueObjects
