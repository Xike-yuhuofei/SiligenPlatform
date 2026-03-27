#pragma once

#include "shared/types/Types.h"

namespace Siligen::Domain::Motion::ValueObjects {

using Siligen::Shared::Types::float32;

struct TimePlanningConfig {
    float32 vmax = 50.0f;
    float32 amax = 500.0f;
    float32 jmax = 0.0f;  // Used for observation; strict only when enforce_jerk_limit is true.
    float32 sample_ds = 0.0f;
    float32 sample_dt = 0.01f;
    float32 curvature_speed_factor = 1.0f;
    float32 corner_speed_factor = 0.6f;
    float32 start_speed_factor = 0.5f;
    float32 end_speed_factor = 0.5f;
    float32 rapid_speed_factor = 1.0f;
    float32 arc_tolerance_mm = 0.0f;
    bool enforce_jerk_limit = true;  // Enable strict jerk limiting via Ruckig (on by default).
};

}  // namespace Siligen::Domain::Motion::ValueObjects
