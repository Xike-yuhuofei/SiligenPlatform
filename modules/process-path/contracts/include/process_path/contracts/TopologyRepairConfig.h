#pragma once

#include "shared/types/Point2D.h"

namespace Siligen::ProcessPath::Contracts {

enum class TopologyRepairPolicy {
    Off,
    Auto
};

struct TopologyRepairConfig {
    TopologyRepairPolicy policy = TopologyRepairPolicy::Off;
    Siligen::Shared::Types::Point2D start_position{};
    int two_opt_iterations = 0;
    bool split_intersections = true;
    bool rebuild_by_connectivity = true;
    bool reorder_contours = true;
};

}  // namespace Siligen::ProcessPath::Contracts
