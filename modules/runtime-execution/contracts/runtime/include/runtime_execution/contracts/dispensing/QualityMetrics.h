#pragma once

#include "shared/types/Types.h"

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

struct QualityMetrics {
    float32 line_width_cv = 0.0f;
    float32 corner_deviation_pct = 0.0f;
    float32 glue_pile_reduction_pct = 0.0f;
    float32 trigger_interval_error_pct = 0.0f;
    int32 run_count = 0;
    int32 interruption_count = 0;
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
