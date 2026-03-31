#pragma once

#include "TopologySpanSplitter.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::float32;
using Siligen::Domain::Dispensing::ValueObjects::TopologyDispatchType;

struct TopologyComponent {
    std::vector<TopologySpanSlice> spans;
    std::vector<std::size_t> source_segment_indices;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    bool ignored = false;
    std::string ignored_reason;
    std::string blocking_reason;
};

struct TopologyComponentClassifierRequest {
    std::vector<TopologySpanSlice> spans;
    float32 vertex_tolerance_mm = 0.0f;
    float32 min_spacing_mm = 0.0f;
};

struct TopologyComponentClassifierResult {
    std::vector<TopologyComponent> components;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    std::size_t effective_component_count = 0;
    std::size_t ignored_component_count = 0;
    bool multi_contour_split_applied = false;
    bool auxiliary_geometry_ignored = false;
};

class TopologyComponentClassifier {
   public:
    TopologyComponentClassifierResult Classify(const TopologyComponentClassifierRequest& request) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
