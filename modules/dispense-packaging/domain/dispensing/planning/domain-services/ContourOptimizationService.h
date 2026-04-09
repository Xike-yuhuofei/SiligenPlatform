#pragma once

#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Primitive.h"
#include "shared/types/Point2D.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

struct ContourOptimizationStats {
    size_t contour_count = 0;
    size_t reordered_contours = 0;
    size_t reversed_contours = 0;
    size_t rotated_contours = 0;
    bool applied = false;
    bool metadata_valid = false;
};

class ContourOptimizationService {
   public:
    static std::vector<Siligen::ProcessPath::Contracts::Primitive> Optimize(
        const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
        const std::vector<Siligen::ProcessPath::Contracts::PathPrimitiveMeta>& metadata,
        const Shared::Types::Point2D& start_pos,
        bool enable,
        int two_opt_iterations = 0,
        ContourOptimizationStats* stats = nullptr);
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


