#pragma once

#include "TopologySpanSplitter.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices::Internal {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpan;

struct ClosedLoopCornerCandidate {
    Point2D position;
    float32 distance_mm_span = 0.0f;
    float32 significance_angle_deg = 0.0f;
    std::size_t source_segment_index = 0;
};

struct ClosedLoopCornerResolution {
    std::vector<ClosedLoopCornerCandidate> accepted_candidates;
    std::size_t candidate_corner_count = 0;
    std::size_t suppressed_corner_count = 0;
    std::size_t dense_corner_cluster_count = 0;
};

struct ClosedLoopCornerAnchorResolverInput {
    const TopologySpanSlice* span = nullptr;
    const DispenseSpan* layout_span = nullptr;
    float32 total_length_mm = 0.0f;
    float32 angle_threshold_deg = 30.0f;
    float32 cluster_distance_mm = 0.0f;
    float32 anchor_tolerance_mm = 0.0f;
    float32 vertex_tolerance_mm = 0.0f;
    bool enabled = true;
};

class ClosedLoopCornerAnchorResolver {
   public:
    ClosedLoopCornerResolution Resolve(const ClosedLoopCornerAnchorResolverInput& input) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices::Internal
