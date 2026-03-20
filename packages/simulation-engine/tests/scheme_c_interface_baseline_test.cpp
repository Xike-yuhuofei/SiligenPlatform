#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

#include "sim/scheme_c/simulation_session.h"
#include "sim/scheme_c/virtual_clock.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testVirtualClockAdvancesDeterministically() {
    sim::scheme_c::VirtualClock clock(std::chrono::milliseconds(1));
    clock.start();
    require(clock.current().tick_index == 0, "virtual clock: initial tick mismatch");
    require(clock.current().now == std::chrono::milliseconds(0), "virtual clock: initial time mismatch");

    clock.advance();
    require(clock.current().tick_index == 1, "virtual clock: first advance mismatch");
    require(clock.current().now == std::chrono::milliseconds(1), "virtual clock: first advance time mismatch");

    clock.advance(4);
    require(clock.current().tick_index == 5, "virtual clock: multi-advance tick mismatch");
    require(clock.current().now == std::chrono::milliseconds(5), "virtual clock: multi-advance time mismatch");
}

void testBaselineSessionFreezesBridgeMetadataAndTickOrder() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y", "Z"};
    config.io_channels = {"DO_VALVE"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 4;

    auto session = sim::scheme_c::createBaselineSession(config);
    const auto result = session.run();

    require(result.status == sim::scheme_c::SessionStatus::Completed, "session: expected completion");
    require(result.recording.snapshots.size() == 1U, "session: expected one snapshot from seam stub");
    require(result.recording.ticks_executed == 1U, "session: expected one executed tick");
    require(result.recording.simulated_time == std::chrono::milliseconds(1), "session: expected one tick of time");
    require(result.recording.snapshots.front().tick.tick_index == 0U, "session: first snapshot tick mismatch");
    require(result.recording.motion_profile.size() == 3U, "session: motion_profile should contain one entry per axis");
    require(result.recording.timeline.size() >= 2U, "session: expected canonical timeline entries");
    require(result.recording.summary.terminal_state == "completed", "session: summary terminal state mismatch");
    require(result.recording.summary.termination_reason == "bridge_completed", "session: summary reason mismatch");
    require(!result.recording.summary.empty_timeline, "session: timeline should not be empty");
    require(result.recording.final_axes.size() == 3U, "session: final axis count mismatch");
    require(result.recording.final_io.size() == 1U, "session: final io count mismatch");
    require(result.runtime_bridge.owner == "packages/simulation-engine/runtime-bridge",
            "session: runtime bridge owner mismatch");
    require(result.runtime_bridge.process_runtime_core_linked,
            "session: process-runtime-core should be linked for baseline session");
    require(
        result.runtime_bridge.next_integration_point.find("SC-B-002 closed") != std::string::npos,
        "session: runtime bridge should report the closed core-owned assembly seam");
    require(result.runtime_bridge.follow_up_seams.empty(),
            "session: expected no remaining follow-up seams");
    require(result.bridge_bindings.axis_bindings.size() == 3U, "session: expected resolved axis bindings");
    require(result.bridge_bindings.io_bindings.size() == 1U, "session: expected resolved io bindings");
    require(result.bridge_bindings.axis_bindings[0].logical_axis_index == 0, "session: X axis binding mismatch");
    require(result.recording.events.size() >= 2U, "session: expected seam events");
}

}  // namespace

int main() {
    try {
        testVirtualClockAdvancesDeterministically();
        testBaselineSessionFreezesBridgeMetadataAndTickOrder();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
