#pragma once

#include "process_path/contracts/Path.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::ProcessPath::Contracts::Segment;

struct FlattenedCurvePath {
    std::vector<Point2D> points;
    std::vector<float32> cumulative_lengths_mm;
    float32 total_length_mm = 0.0f;
};

class CurveFlatteningService {
   public:
    Result<FlattenedCurvePath> Flatten(
        const Segment& segment,
        float32 max_error_mm,
        float32 max_step_mm = 0.0f) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
