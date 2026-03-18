#include "sim/planner/lookahead_planner.h"

#include <algorithm>
#include <cmath>

namespace sim {

std::vector<PlannedSegment> LookaheadPlanner::plan(const Trajectory& trajectory,
                                                   double max_velocity,
                                                   double max_acceleration) const {
    const std::size_t count = trajectory.segmentCount();
    std::vector<PlannedSegment> plan(count);
    if (count == 0) {
        return plan;
    }

    std::vector<double> brake_limited_start(count + 1, 0.0);
    brake_limited_start[count] = 0.0;

    for (std::size_t offset = 0; offset < count; ++offset) {
        const std::size_t index = count - 1U - offset;
        const double length = trajectory.segment(index).length();
        brake_limited_start[index] = std::min(
            max_velocity,
            std::sqrt((brake_limited_start[index + 1] * brake_limited_start[index + 1]) +
                      (2.0 * max_acceleration * length)));
    }

    double entry_velocity = 0.0;
    for (std::size_t index = 0; index < count; ++index) {
        const double length = trajectory.segment(index).length();
        const double accel_limited_exit =
            std::sqrt((entry_velocity * entry_velocity) + (2.0 * max_acceleration * length));
        const double exit_velocity =
            std::min({max_velocity, brake_limited_start[index + 1], accel_limited_exit});
        const auto profile = VelocityProfile::compute(
            length,
            entry_velocity,
            exit_velocity,
            max_velocity,
            max_acceleration);

        plan[index] = PlannedSegment{
            length,
            entry_velocity,
            exit_velocity,
            profile.peak_velocity,
            profile.total_time
        };
        entry_velocity = exit_velocity;
    }

    return plan;
}

}  // namespace sim
