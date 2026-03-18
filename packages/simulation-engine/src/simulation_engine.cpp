#include "sim/simulation_engine.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "sim/control/io_model.h"
#include "sim/control/trigger_model.h"
#include "sim/device/valve_model.h"
#include "sim/motion/axis_model.h"
#include "sim/motion/trajectory_executor.h"
#include "sim/planner/lookahead_planner.h"
#include "sim/simulation_clock.h"

namespace sim {

SimulationResult SimulationEngine::run(const SimulationContext& context) const {
    SimulationResult result{};

    SimulationClock clock(context.timestep_seconds);
    AxisModel axis;
    axis.configure(context.motion_limits.max_acceleration);

    LookaheadPlanner planner;
    auto plan = planner.plan(context.trajectory,
                             context.motion_limits.max_velocity,
                             context.motion_limits.max_acceleration);

    TrajectoryExecutor executor;
    executor.initialize(context.trajectory, std::move(plan), context.motion_limits.max_acceleration);

    IOModel io_model;
    for (const auto& delay : context.io_delays) {
        io_model.setDelay(delay.channel, delay.delay_seconds);
    }

    TriggerModel trigger_model;
    trigger_model.initialize(context.triggers);

    ValveModel valve_model;
    valve_model.configure(context.valve);

    const std::size_t max_steps = static_cast<std::size_t>(
        std::max(1.0, context.max_simulation_time_seconds / context.timestep_seconds));

    for (std::size_t step = 0; step < max_steps; ++step) {
        const double now = clock.now();
        const double dt = clock.timestep();

        const auto command = executor.update(axis.position(), axis.velocity());
        axis.update(command.target_velocity, dt);

        trigger_model.update(axis.position(), now, io_model);
        io_model.update(now);

        valve_model.setCommand(io_model.getOutput(channels::kValve), now);
        valve_model.update(now, dt);

        result.motion_profile.push_back(MotionSample{
            now,
            axis.position(),
            axis.velocity(),
            command.target_velocity,
            command.segment_index,
            command.segment_progress
        });

        const bool motion_complete = executor.isComplete(axis.position());
        const bool axis_stopped = std::abs(axis.velocity()) < 1e-4;
        if (motion_complete && axis_stopped && !io_model.hasPendingEvents() && !valve_model.hasPendingEvents()) {
            clock.step();
            break;
        }

        clock.step();
    }

    result.total_time = clock.now();
    result.motion_distance = axis.position();
    result.valve_open_time = valve_model.valveOpenTime();
    result.io_events = io_model.events();
    result.valve_events = valve_model.events();

    return result;
}

}  // namespace sim
