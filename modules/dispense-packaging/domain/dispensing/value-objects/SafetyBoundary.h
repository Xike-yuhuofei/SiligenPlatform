#pragma once

#include "shared/types/Types.h"

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::int32;

struct SafetyBoundary {
    int32 duration_ms = 0;
    int32 valve_response_ms = 0;
    int32 margin_ms = 0;
    int32 min_interval_ms = 0;
    bool downgrade_on_violation = false;

    bool IsSafe() const noexcept {
        return min_interval_ms >= duration_ms + valve_response_ms;
    }
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
