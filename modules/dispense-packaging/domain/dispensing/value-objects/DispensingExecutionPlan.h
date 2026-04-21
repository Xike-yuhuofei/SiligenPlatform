#pragma once

#include "motion_planning/contracts/MotionTrajectory.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "shared/types/DispensingExecutionSemantics.h"
#include "shared/types/Point.h"
#include "shared/types/TrajectoryTriggerUtils.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::kDefaultTriggerPulseWidthUs;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::uint16;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::TrajectoryPoint;
using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;

enum class ProductionTriggerMode {
    NONE = 0,
    PROFILE_COMPARE,
};

inline const char* ToString(ProductionTriggerMode mode) noexcept {
    switch (mode) {
        case ProductionTriggerMode::NONE:
            return "none";
        case ProductionTriggerMode::PROFILE_COMPARE:
            return "profile_compare";
    }
    return "none";
}

struct ProfileCompareTriggerPoint {
    uint32 sequence_index = 0;
    float32 profile_position_mm = 0.0f;
    Point2D trigger_position_mm{};
    uint16 pulse_width_us = kDefaultTriggerPulseWidthUs;
};

struct ProfileCompareProgramSpan {
    uint32 trigger_begin_index = 0;
    uint32 trigger_end_index = 0;
    short compare_source_axis = 0;
    uint32 start_boundary_trigger_count = 0;
    float32 start_profile_position_mm = 0.0f;
    float32 end_profile_position_mm = 0.0f;
    Point2D start_position_mm{};
    Point2D end_position_mm{};

    [[nodiscard]] uint32 ExpectedTriggerCount() const noexcept {
        if (trigger_end_index < trigger_begin_index) {
            return 0U;
        }
        return trigger_end_index - trigger_begin_index + 1U;
    }
};

struct ProfileCompareTriggerProgram {
    std::vector<ProfileCompareTriggerPoint> trigger_points;
    std::vector<ProfileCompareProgramSpan> spans;
    uint32 expected_trigger_count = 0;

    [[nodiscard]] bool Empty() const noexcept {
        return trigger_points.empty();
    }
};

struct ProductionCompletionContract {
    bool enabled = false;
    Point2D final_target_position_mm{};
    float32 final_position_tolerance_mm = 0.0f;
    uint32 expected_trigger_count = 0;
};

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
    ProductionTriggerMode production_trigger_mode = ProductionTriggerMode::NONE;
    ProfileCompareTriggerProgram profile_compare_program;
    ProductionCompletionContract completion_contract;

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
