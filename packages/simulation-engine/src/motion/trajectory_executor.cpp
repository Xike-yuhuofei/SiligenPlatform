#include "sim/motion/trajectory_executor.h"

#include <algorithm>
#include <cmath>

namespace sim {

void TrajectoryExecutor::initialize(const Trajectory& trajectory,
                                    std::vector<PlannedSegment> planned_segments,
                                    double max_acceleration) {
    trajectory_ = &trajectory;
    planned_segments_ = std::move(planned_segments);
    segment_starts_ = trajectory.cumulativeLengths();
    total_length_ = trajectory.totalLength();
    max_acceleration_ = max_acceleration;
}

ExecutorCommand TrajectoryExecutor::update(double axis_position, double axis_velocity) {
    if (!trajectory_ || planned_segments_.empty()) {
        return ExecutorCommand{};
    }

    const double clamped_position = std::clamp(axis_position, 0.0, total_length_);
    if (isComplete(clamped_position)) {
        return ExecutorCommand{0.0, planned_segments_.size() - 1U, 1.0, true};
    }

    std::size_t segment_index = 0;
    for (std::size_t index = 0; index + 1 < segment_starts_.size(); ++index) {
        if (clamped_position >= segment_starts_[index + 1]) {
            segment_index = index + 1;
        } else {
            break;
        }
    }

    const auto& segment = planned_segments_[segment_index];
    const double segment_start = segment_starts_[segment_index];
    const double local_distance = std::max(0.0, clamped_position - segment_start);
    const double remaining_distance = std::max(0.0, segment.length - local_distance);
    const double braking_velocity = std::sqrt(
        std::max(0.0, (segment.exit_velocity * segment.exit_velocity) +
                           (2.0 * max_acceleration_ * remaining_distance)));
    const double target_velocity = std::min(segment.cruise_velocity, braking_velocity);
    const double progress =
        (segment.length > 1e-9) ? std::clamp(local_distance / segment.length, 0.0, 1.0) : 1.0;

    return ExecutorCommand{
        std::max(target_velocity, axis_velocity > target_velocity ? target_velocity : 0.0),
        segment_index,
        progress,
        false
    };
}

bool TrajectoryExecutor::isComplete(double axis_position) const {
    return (!trajectory_) || (axis_position >= (total_length_ - 1e-6));
}

double TrajectoryExecutor::totalLength() const {
    return total_length_;
}

}  // namespace sim
