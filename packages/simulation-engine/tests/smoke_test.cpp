#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "sim/control/io_model.h"
#include "sim/simulation_engine.h"
#include "sim/trajectory/line_segment.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

sim::SimulationContext makeBaseContext() {
    sim::SimulationContext context;
    context.timestep_seconds = 0.001;
    context.max_simulation_time_seconds = 5.0;
    context.motion_limits.max_velocity = 100.0;
    context.motion_limits.max_acceleration = 200.0;
    context.valve = sim::ValveConfig{0.005, 0.005};
    return context;
}

void testBasicMotionAndValveChain() {
    auto context = makeBaseContext();
    context.trajectory.addSegment(std::make_unique<sim::LineSegment>(
        sim::Point2d{0.0, 0.0},
        sim::Point2d{100.0, 0.0}));

    context.io_delays.push_back(sim::OutputDelaySpec{sim::channels::kValve, 0.001});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, true});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 80.0, false});

    sim::SimulationEngine engine;
    const auto result = engine.run(context);

    require(result.total_time > 0.0, "basic: total_time should be positive");
    require(result.motion_distance >= 99.0, "basic: motion distance should reach target");
    require(!result.motion_profile.empty(), "basic: motion profile should not be empty");
    require(!result.io_events.empty(), "basic: io events should not be empty");
    require(result.valve_open_time > 0.0, "basic: valve open time should be positive");
}

void testEmptyTrajectoryTerminatesDeterministically() {
    auto context = makeBaseContext();

    sim::SimulationEngine engine;
    const auto result = engine.run(context);

    require(result.total_time == context.timestep_seconds, "empty: total time should be one timestep");
    require(result.motion_distance == 0.0, "empty: motion distance should stay zero");
    require(result.motion_profile.size() == 1U, "empty: should emit one sample");
    require(result.io_events.empty(), "empty: io events should be empty");
    require(result.valve_events.empty(), "empty: valve events should be empty");
}

void testVeryShortSegmentStillCompletes() {
    auto context = makeBaseContext();
    context.motion_limits.max_velocity = 50.0;
    context.motion_limits.max_acceleration = 500.0;
    context.trajectory.addSegment(std::make_unique<sim::LineSegment>(
        sim::Point2d{0.0, 0.0},
        sim::Point2d{0.05, 0.0}));

    sim::SimulationEngine engine;
    const auto result = engine.run(context);

    require(result.total_time > 0.0, "short segment: total time should be positive");
    require(result.motion_distance >= 0.049, "short segment: should reach end of short move");
    require(result.motion_profile.size() >= 1U, "short segment: should produce samples");
}

void testConsecutiveValveTogglesProduceOrderedEvents() {
    auto context = makeBaseContext();
    context.motion_limits.max_velocity = 120.0;
    context.motion_limits.max_acceleration = 300.0;
    context.valve = sim::ValveConfig{0.002, 0.002};
    context.trajectory.addSegment(std::make_unique<sim::LineSegment>(
        sim::Point2d{0.0, 0.0},
        sim::Point2d{120.0, 0.0}));

    context.io_delays.push_back(sim::OutputDelaySpec{sim::channels::kValve, 0.001});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, true});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 20.0, false});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 30.0, true});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 40.0, false});

    sim::SimulationEngine engine;
    const auto result = engine.run(context);

    require(result.io_events.size() == 4U, "toggle: expected four io events");
    require(result.valve_events.size() == 4U, "toggle: expected four valve events");
    require(result.io_events[0].state == true, "toggle: first io event should open");
    require(result.io_events[1].state == false, "toggle: second io event should close");
    require(result.valve_events[2].state == true, "toggle: third valve event should open");
    require(result.valve_events[3].state == false, "toggle: fourth valve event should close");
    require(result.valve_events[0].time <= result.valve_events[1].time &&
                result.valve_events[1].time <= result.valve_events[2].time &&
                result.valve_events[2].time <= result.valve_events[3].time,
            "toggle: valve event timestamps should be ordered");
}

void testCoincidentTriggersCollapseDuplicateCommands() {
    auto context = makeBaseContext();
    context.valve = sim::ValveConfig{0.001, 0.001};
    context.trajectory.addSegment(std::make_unique<sim::LineSegment>(
        sim::Point2d{0.0, 0.0},
        sim::Point2d{60.0, 0.0}));

    context.io_delays.push_back(sim::OutputDelaySpec{sim::channels::kValve, 0.0});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, true});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, true});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, false});
    context.triggers.push_back(sim::TriggerSpec{sim::channels::kValve, 10.0, false});

    sim::SimulationEngine engine;
    const auto result = engine.run(context);

    require(result.io_events.size() == 2U, "coincident triggers: duplicate commands should collapse to two io events");
    require(result.io_events[0].state == true, "coincident triggers: first io event should open");
    require(result.io_events[1].state == false, "coincident triggers: second io event should close");
    require(result.valve_events.empty(),
            "coincident triggers: same-cycle open/close should not produce physical valve toggles");
}

}  // namespace

int main() {
    try {
        testBasicMotionAndValveChain();
        testEmptyTrajectoryTerminatesDeterministically();
        testVeryShortSegmentStillCompletes();
        testConsecutiveValveTogglesProduceOrderedEvents();
        testCoincidentTriggersCollapseDuplicateCommands();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
