#pragma once

#include "domain/trajectory/ports/IPathSourcePort.h"
#include "domain/trajectory/value-objects/Primitive.h"
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
    static std::vector<Domain::Trajectory::ValueObjects::Primitive> Optimize(
        const std::vector<Domain::Trajectory::ValueObjects::Primitive>& primitives,
        const std::vector<Domain::Trajectory::Ports::PathPrimitiveMeta>& metadata,
        const Shared::Types::Point2D& start_pos,
        bool enable,
        int two_opt_iterations = 0,
        ContourOptimizationStats* stats = nullptr);
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


