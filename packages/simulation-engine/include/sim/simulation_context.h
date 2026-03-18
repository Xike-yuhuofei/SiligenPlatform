#pragma once

#include <string>
#include <vector>

#include "sim/trajectory/trajectory.h"

namespace sim {

struct MotionLimits {
    double max_velocity{200.0};
    double max_acceleration{500.0};
};

struct OutputDelaySpec {
    std::string channel;
    double delay_seconds{0.0};
};

struct TriggerSpec {
    std::string channel;
    double trigger_position{0.0};
    bool state{true};
};

struct ValveConfig {
    double open_delay_seconds{0.015};
    double close_delay_seconds{0.010};
};

struct SimulationContext {
    Trajectory trajectory;
    MotionLimits motion_limits{};
    std::vector<OutputDelaySpec> io_delays{};
    std::vector<TriggerSpec> triggers{};
    ValveConfig valve{};
    double timestep_seconds{0.001};
    double max_simulation_time_seconds{600.0};
};

}  // namespace sim
