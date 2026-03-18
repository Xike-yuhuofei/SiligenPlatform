#pragma once

#include <vector>

#include "sim/planner/velocity_profile.h"
#include "sim/trajectory/trajectory.h"

namespace sim {

struct PlannedSegment {
    double length{0.0};
    double entry_velocity{0.0};
    double exit_velocity{0.0};
    double cruise_velocity{0.0};
    double segment_time{0.0};
};

class LookaheadPlanner {
public:
    std::vector<PlannedSegment> plan(const Trajectory& trajectory,
                                     double max_velocity,
                                     double max_acceleration) const;
};

}  // namespace sim
