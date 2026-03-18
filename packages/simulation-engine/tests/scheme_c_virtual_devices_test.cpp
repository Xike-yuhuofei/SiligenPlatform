#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_io.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

sim::scheme_c::TickInfo secondsTick(sim::scheme_c::TickIndex tick_index, int seconds) {
    return sim::scheme_c::TickInfo{
        tick_index,
        std::chrono::seconds(seconds),
        std::chrono::seconds(seconds)
    };
}

void testAxisMotionFeedbackIsDeterministic() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});

    sim::scheme_c::AxisState x_state = axis_group->readAxis("X");
    x_state.enabled = true;
    x_state.configured_velocity_mm_per_s = 5.0;
    axis_group->writeAxisState(x_state);

    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 20.0, 5.0, false, false}});

    const auto before_tick = axis_group->readAxis("X");
    require(before_tick.position_mm == 0.0, "virtual axis: command should not update position before tick");
    require(!before_tick.running, "virtual axis: command should not set running before tick");
    require(before_tick.done, "virtual axis: axis should remain done before tick");

    axis_group->advance(secondsTick(0, 1));
    const auto after_first_tick = axis_group->readAxis("X");
    require(after_first_tick.position_mm == 5.0, "virtual axis: first tick position mismatch");
    require(after_first_tick.velocity_mm_per_s == 5.0, "virtual axis: first tick velocity mismatch");
    require(after_first_tick.running, "virtual axis: axis should be running after first tick");
    require(!after_first_tick.done, "virtual axis: axis should not be done during motion");

    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 20.0, 5.0, false, false}});
    axis_group->advance(secondsTick(1, 1));
    const auto after_second_tick = axis_group->readAxis("X");
    require(after_second_tick.position_mm == 10.0, "virtual axis: duplicate command should not restart motion");
    require(after_second_tick.running, "virtual axis: axis should keep running on duplicate command");

    axis_group->advance(secondsTick(2, 1));
    axis_group->advance(secondsTick(3, 1));
    const auto completed = axis_group->readAxis("X");
    require(completed.position_mm == 20.0, "virtual axis: final position mismatch");
    require(completed.velocity_mm_per_s == 0.0, "virtual axis: final velocity should be zero");
    require(!completed.running, "virtual axis: axis should stop after reaching target");
    require(completed.done, "virtual axis: axis should report done after reaching target");
}

void testAxisHomeAndSoftLimitSemantics() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});

    sim::scheme_c::AxisState x_state = axis_group->readAxis("X");
    x_state.enabled = true;
    x_state.position_mm = 2.0;
    x_state.configured_velocity_mm_per_s = 1.0;
    x_state.soft_limit_negative_mm = -1.0;
    x_state.soft_limit_positive_mm = 3.0;
    axis_group->writeAxisState(x_state);

    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 0.0, 1.0, true, false}});
    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 0.0, 1.0, true, false}});

    axis_group->advance(secondsTick(0, 1));
    const auto homing_tick = axis_group->readAxis("X");
    require(homing_tick.position_mm == 1.0, "virtual axis: homing should move toward zero");
    require(homing_tick.running, "virtual axis: homing should report running");
    require(!homing_tick.homed, "virtual axis: axis should not be homed mid-motion");

    axis_group->advance(secondsTick(1, 1));
    const auto homed = axis_group->readAxis("X");
    require(homed.position_mm == 0.0, "virtual axis: homed position mismatch");
    require(homed.homed, "virtual axis: axis should be homed after reaching zero");
    require(homed.home_signal, "virtual axis: home signal should be active at zero");
    require(homed.index_signal, "virtual axis: index signal should pulse when crossing zero");
    require(!homed.running && homed.done, "virtual axis: homing should complete deterministically");

    axis_group->applyCommands({sim::scheme_c::AxisCommand{"X", 10.0, 5.0, false, false}});
    axis_group->advance(secondsTick(2, 1));
    const auto limited = axis_group->readAxis("X");
    require(limited.position_mm == 3.0, "virtual axis: move should clamp to positive soft limit");
    require(limited.positive_soft_limit, "virtual axis: positive soft limit should be active");
    require(limited.has_error, "virtual axis: out-of-range move should raise error");
    require(limited.error_code != 0, "virtual axis: out-of-range move should set error code");
    require(!limited.running && limited.done, "virtual axis: out-of-range move should complete on limit");
}

void testOptionalZAndVirtualIoSemantics() {
    auto axis_group = sim::scheme_c::createInMemoryVirtualAxisGroup({"X", "Y"});
    auto io = sim::scheme_c::createInMemoryVirtualIo({"DO_VALVE", "DI_HOME"});

    require(!axis_group->hasAxis("Z"), "virtual axis: Z should be optional");
    const auto z_state = axis_group->readAxis("Z");
    require(z_state.axis == "Z", "virtual axis: missing axis should preserve requested name");
    require(z_state.position_mm == 0.0, "virtual axis: missing axis should read default position");
    require(!z_state.homed, "virtual axis: missing axis should not appear homed");

    io->writeOutput("DO_VALVE", true);
    require(!io->readSignal("DO_VALVE"), "virtual io: output should not flip before advance");

    io->advance(secondsTick(0, 1));
    require(io->readSignal("DO_VALVE"), "virtual io: output should flip on advance");

    io->writeOutput("DO_VALVE", true);
    io->advance(secondsTick(1, 1));
    require(io->readSignal("DO_VALVE"), "virtual io: duplicate output command should be idempotent");

    io->writeChannelState(sim::scheme_c::IoState{"DI_HOME", true, false});
    const auto input_state = io->readChannel("DI_HOME");
    require(input_state.value, "virtual io: direct input injection should be readable");
    require(!input_state.output, "virtual io: injected input should preserve direction");
}

}  // namespace

int main() {
    try {
        testAxisMotionFeedbackIsDeterministic();
        testAxisHomeAndSoftLimitSemantics();
        testOptionalZAndVirtualIoSemantics();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
