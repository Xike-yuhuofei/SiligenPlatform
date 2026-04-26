#pragma once

#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::Segment;

struct ArcLengthLocation {
    Point2D position;
    std::size_t segment_index = 0;
};

class PathArcLengthLocator {
   public:
    Result<ArcLengthLocation> Locate(
        const ProcessPath& path,
        float32 distance_mm,
        float32 curve_flatten_max_error_mm = 0.0f,
        float32 curve_flatten_max_step_mm = 0.0f) const;

    Result<ArcLengthLocation> Locate(
        const std::vector<ProcessSegment>& segments,
        float32 distance_mm,
        float32 curve_flatten_max_error_mm = 0.0f,
        float32 curve_flatten_max_step_mm = 0.0f) const;

    Result<Point2D> LocateOnSegment(
        const Segment& segment,
        float32 local_distance_mm,
        float32 curve_flatten_max_error_mm = 0.0f,
        float32 curve_flatten_max_step_mm = 0.0f) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
