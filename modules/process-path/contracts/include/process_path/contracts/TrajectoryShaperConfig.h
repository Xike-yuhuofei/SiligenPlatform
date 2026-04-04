#pragma once

#include "shared/types/Types.h"

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::float32;

struct TrajectoryShaperConfig {
    float32 corner_smoothing_radius = 0.5f;
    float32 corner_max_deviation_mm = 0.1f;
    float32 corner_min_radius_mm = 0.1f;
    float32 position_tolerance = 0.1f;
};

}  // namespace Siligen::ProcessPath::Contracts
