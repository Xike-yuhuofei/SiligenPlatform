#pragma once

#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;

struct TopologyStrongAnchorCandidate {
    Point2D position;
    float32 distance_mm_span = 0.0f;
    std::size_t source_segment_index = 0;
    StrongAnchorRole role = StrongAnchorRole::SplitBoundary;
};

struct TopologySpanSlice {
    std::vector<ProcessSegment> segments;
    std::vector<std::size_t> source_segment_indices;
    std::vector<float32> segment_lengths_mm;
    float32 start_distance_mm = 0.0f;
    DispenseSpanSplitReason split_reason = DispenseSpanSplitReason::None;
    std::vector<TopologyStrongAnchorCandidate> strong_anchor_candidates;
};

struct TopologySpanSplitterRequest {
    ProcessPath process_path;
    std::vector<float32> segment_lengths_mm;
    float32 vertex_tolerance_mm = 0.0f;
    bool enable_branch_revisit_split = true;
};

struct TopologySpanSplitterResult {
    std::vector<TopologySpanSlice> spans;
    bool topology_dispatch_applied = false;
    bool branch_revisit_split_applied = false;
};

class TopologySpanSplitter {
   public:
    TopologySpanSplitterResult Split(const TopologySpanSplitterRequest& request) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
