#pragma once

#include "shared/types/Types.h"

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::float32;

struct DispenseCompensationProfile {
    float32 open_comp_ms = 0.0f;
    float32 close_comp_ms = 0.0f;
    bool retract_enabled = false;
    float32 corner_pulse_scale = 1.0f;
    float32 curvature_speed_factor = 0.8f;
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
