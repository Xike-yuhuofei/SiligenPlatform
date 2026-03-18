#pragma once

#include "SafetyBoundary.h"
#include "shared/types/DispensingStrategy.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;

struct TriggerPlan {
    DispensingStrategy strategy = DispensingStrategy::BASELINE;
    float32 interval_mm = 0.0f;
    int32 subsegment_count = 0;
    bool dispense_only_cruise = false;
    SafetyBoundary safety;
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
