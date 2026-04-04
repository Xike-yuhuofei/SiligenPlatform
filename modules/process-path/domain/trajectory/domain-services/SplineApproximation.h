#pragma once

#include "process_path/contracts/Path.h"
#include "process_path/contracts/Primitive.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Shared::Types::float32;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SplinePrimitive;

struct SplineApproximationConfig {
    float32 max_step_mm = 0.0f;
    float32 max_error_mm = 0.0f;
    float32 min_segment_mm = 0.0f;
};

class SplineApproximation {
   public:
    SplineApproximation() = default;
    ~SplineApproximation() = default;

    std::vector<Segment> Approximate(const SplinePrimitive& spline, const SplineApproximationConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
