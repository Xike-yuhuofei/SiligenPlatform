#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "sim/simulation_context.h"

namespace sim {

struct MotionSample {
    double time{0.0};
    double position{0.0};
    double velocity{0.0};
    double target_velocity{0.0};
    std::size_t segment_index{0};
    double segment_progress{0.0};
};

struct TimelineEvent {
    double time{0.0};
    std::string channel;
    bool state{false};
    std::string source;
};

struct SimulationResult {
    double total_time{0.0};
    double motion_distance{0.0};
    double valve_open_time{0.0};
    std::vector<MotionSample> motion_profile{};
    std::vector<TimelineEvent> io_events{};
    std::vector<TimelineEvent> valve_events{};
};

class SimulationEngine {
public:
    SimulationResult run(const SimulationContext& context) const;
};

}  // namespace sim
