#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::TrajectoryPoint;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;

struct DispensingExecutionPlan {
    std::vector<InterpolationData> interpolation_segments;
    std::vector<TrajectoryPoint> interpolation_points;
    MotionTrajectory motion_trajectory;
    std::vector<float32> trigger_distances_mm;
    uint32 trigger_interval_ms = 0;
    float32 trigger_interval_mm = 0.0f;
    float32 total_length_mm = 0.0f;
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
