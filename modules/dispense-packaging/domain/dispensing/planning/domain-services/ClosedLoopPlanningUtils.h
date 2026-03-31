#pragma once

#include "shared/types/Types.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices::Internal {

using Siligen::Shared::Types::float32;

inline constexpr float32 kClosedLoopPlanningEpsilon = 1e-6f;

inline float32 WrapLoopDistance(float32 distance_mm, float32 total_length_mm) {
    if (total_length_mm <= kClosedLoopPlanningEpsilon) {
        return 0.0f;
    }
    const float32 wrapped = std::fmod(distance_mm, total_length_mm);
    return wrapped < 0.0f ? wrapped + total_length_mm : wrapped;
}

inline float32 CircularDistance(float32 lhs, float32 rhs, float32 total_length_mm) {
    const float32 direct = std::fabs(lhs - rhs);
    return std::min(direct, std::max(0.0f, total_length_mm - direct));
}

}  // namespace Siligen::Domain::Dispensing::DomainServices::Internal
