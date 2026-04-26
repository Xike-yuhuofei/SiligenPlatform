#pragma once

#include "shared/types/Types.h"

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::float32;

struct NormalizationConfig {
    float32 unit_scale = 1.0f;
    float32 continuity_tolerance = 0.1f;
    float32 curve_flatten_max_step_mm = 0.0f;
    float32 curve_flatten_max_error_mm = 0.0f;
};

struct NormalizationReport {
    int discontinuity_count = 0;
    bool closed = false;
    bool invalid_unit_scale = false;
    int skipped_spline_count = 0;
    int point_primitive_count = 0;
    int consumable_segment_count = 0;
};

}  // namespace Siligen::ProcessPath::Contracts
