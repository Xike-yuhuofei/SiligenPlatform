#pragma once

#include "shared/types/Types.h"

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::float32;

struct ProcessConfig {
    float32 default_flow = 1.0f;
    float32 lead_on_dist = 0.0f;
    float32 lead_off_dist = 0.0f;
    bool corner_slowdown = true;
    float32 corner_angle_deg = 45.0f;
    float32 curve_chain_angle_deg = 0.0f;
    float32 curve_chain_max_segment_mm = 0.0f;
    float32 rapid_gap_threshold = 0.1f;
    float32 start_speed_factor = 0.5f;
    float32 end_speed_factor = 0.5f;
    float32 corner_speed_factor = 0.6f;
    float32 rapid_speed_factor = 1.0f;
};

}  // namespace Siligen::ProcessPath::Contracts
