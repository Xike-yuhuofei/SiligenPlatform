#include <chrono>
#include <cmath>
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

bool nearlyEqual(double lhs, double rhs, double epsilon = 1e-9) {
    return std::abs(lhs - rhs) <= epsilon;
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

void testAxisMotionRealismAddsDeterministicFollowingErrorAndQuantization() {
    sim::scheme_c::MotionRealismConfig realism;
    realism.enabled = true;
    realism.servo_lag_seconds = 4.0;
    realism.encoder_quantization_mm = 0.5;

    auto axis_group_a = sim::scheme_c::createInMemoryVirtualAxisGroup({"X"}, realism);
    auto axis_group_b = sim::scheme_c::createInMemoryVirtualAxisGroup({"X"}, realism);

    sim::scheme_c::AxisState x_state{"X"};
    x_state.enabled = true;
    x_state.configured_velocity_mm_per_s = 6.0;
    axis_group_a->writeAxisState(x_state);
    axis_group_b->writeAxisState(x_state);

    const sim::scheme_c::AxisCommand move_command{"X", 6.0, 6.0, false, false};
    axis_group_a->applyCommands({move_command});
    axis_group_b->applyCommands({move_command});

    axis_group_a->advance(secondsTick(0, 1));
    axis_group_b->advance(secondsTick(0, 1));

    const auto first_tick_a = axis_group_a->readAxis("X");
    const auto first_tick_b = axis_group_b->readAxis("X");
    require(nearlyEqual(first_tick_a.position_mm, 1.5), "virtual axis realism: first feedback position mismatch");
    require(nearlyEqual(first_tick_a.velocity_mm_per_s, 1.5), "virtual axis realism: first feedback velocity mismatch");
    require(nearlyEqual(first_tick_a.command_position_mm, 6.0), "virtual axis realism: command position mismatch");
    require(nearlyEqual(first_tick_a.command_velocity_mm_per_s, 0.0), "virtual axis realism: command velocity mismatch");
    require(nearlyEqual(first_tick_a.following_error_mm, 4.5), "virtual axis realism: following error mismatch");
    require(nearlyEqual(first_tick_a.encoder_quantization_mm, 0.5), "virtual axis realism: quantization mismatch");
    require(first_tick_a.following_error_active, "virtual axis realism: following error should be active");
    require(first_tick_a.running, "virtual axis realism: axis should still report moving while settling");
    require(!first_tick_a.done, "virtual axis realism: axis should not be done while settling");
    require(first_tick_a.position_mm == first_tick_b.position_mm &&
                first_tick_a.following_error_mm == first_tick_b.following_error_mm &&
                first_tick_a.following_error_active == first_tick_b.following_error_active,
            "virtual axis realism: repeated execution should stay deterministic on first tick");

    for (sim::scheme_c::TickIndex tick = 1; tick < 16; ++tick) {
        axis_group_a->advance(secondsTick(tick, 1));
        axis_group_b->advance(secondsTick(tick, 1));

        const auto state_a = axis_group_a->readAxis("X");
        const auto state_b = axis_group_b->readAxis("X");
        require(state_a.position_mm == state_b.position_mm, "virtual axis realism: position should stay deterministic");
        require(state_a.velocity_mm_per_s == state_b.velocity_mm_per_s, "virtual axis realism: velocity should stay deterministic");
        require(state_a.following_error_mm == state_b.following_error_mm,
                "virtual axis realism: following error should stay deterministic");
        require(state_a.following_error_active == state_b.following_error_active,
                "virtual axis realism: following error flag should stay deterministic");
    }

    const auto settled = axis_group_a->readAxis("X");
    require(nearlyEqual(settled.position_mm, 6.0), "virtual axis realism: settled feedback position mismatch");
    require(nearlyEqual(settled.command_position_mm, 6.0), "virtual axis realism: settled command position mismatch");
    require(nearlyEqual(settled.following_error_mm, 0.0), "virtual axis realism: settled following error mismatch");
    require(!settled.following_error_active, "virtual axis realism: following error should clear after settling");
    require(!settled.running, "virtual axis realism: axis should stop after settling");
    require(settled.done, "virtual axis realism: axis should finish after settling");
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
        testAxisMotionRealismAddsDeterministicFollowingErrorAndQuantization();
        testAxisHomeAndSoftLimitSemantics();
        testOptionalZAndVirtualIoSemantics();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
