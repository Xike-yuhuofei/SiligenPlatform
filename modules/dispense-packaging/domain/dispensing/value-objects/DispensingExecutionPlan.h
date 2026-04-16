#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/DispensingExecutionSemantics.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::TrajectoryPoint;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;

struct DispensingExecutionPlan {
    DispensingExecutionGeometryKind geometry_kind = DispensingExecutionGeometryKind::PATH;
    DispensingExecutionStrategy execution_strategy = DispensingExecutionStrategy::FLYING_SHOT;
    std::vector<InterpolationData> interpolation_segments;
    std::vector<TrajectoryPoint> interpolation_points;
    MotionTrajectory motion_trajectory;
    std::vector<float32> trigger_distances_mm;
    uint32 trigger_interval_ms = 0;
    float32 trigger_interval_mm = 0.0f;
    float32 total_length_mm = 0.0f;

    [[nodiscard]] bool IsPointGeometry() const noexcept {
        return geometry_kind == DispensingExecutionGeometryKind::POINT;
    }

    [[nodiscard]] bool HasFormalTrajectory() const noexcept {
        return !interpolation_segments.empty() ||
               interpolation_points.size() >= 2U ||
               motion_trajectory.points.size() >= 2U;
    }

    [[nodiscard]] bool HasSinglePointTarget() const noexcept {
        return interpolation_points.size() == 1U ||
               motion_trajectory.points.size() == 1U;
    }

    [[nodiscard]] bool RequiresStationaryPointShot() const noexcept {
        return IsPointGeometry() &&
               execution_strategy == DispensingExecutionStrategy::STATIONARY_SHOT;
    }
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
