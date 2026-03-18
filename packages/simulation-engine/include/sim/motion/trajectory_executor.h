#pragma once

#include <cstddef>
#include <vector>

#include "sim/planner/lookahead_planner.h"
#include "sim/trajectory/trajectory.h"

namespace sim {

struct ExecutorCommand {
    double target_velocity{0.0};
    std::size_t segment_index{0};
    double segment_progress{1.0};
    bool complete{true};
};

class TrajectoryExecutor {
public:
    void initialize(const Trajectory& trajectory,
                    std::vector<PlannedSegment> planned_segments,
                    double max_acceleration);

    ExecutorCommand update(double axis_position, double axis_velocity);
    bool isComplete(double axis_position) const;
    double totalLength() const;

private:
    const Trajectory* trajectory_{nullptr};
    std::vector<PlannedSegment> planned_segments_{};
    std::vector<double> segment_starts_{};
    double total_length_{0.0};
    double max_acceleration_{500.0};
};

}  // namespace sim
