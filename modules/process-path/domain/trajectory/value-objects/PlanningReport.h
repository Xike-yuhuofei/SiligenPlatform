#pragma once

#include "shared/types/Types.h"

namespace Siligen::Domain::Trajectory::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

struct PlanningReport {
    float32 total_length_mm = 0.0f;
    float32 total_time_s = 0.0f;
    float32 max_velocity_observed = 0.0f;
    float32 max_acceleration_observed = 0.0f;
    float32 max_jerk_observed = 0.0f;
    int32 constraint_violations = 0;
    float32 time_integration_error_s = 0.0f;
    int32 time_integration_fallbacks = 0;
    bool jerk_limit_enforced = false;
    bool jerk_plan_failed = false;
    int32 segment_count = 0;
    int32 rapid_segment_count = 0;
    float32 rapid_length_mm = 0.0f;
    int32 corner_segment_count = 0;
    int32 discontinuity_count = 0;
};

}  // namespace Siligen::Domain::Trajectory::ValueObjects
