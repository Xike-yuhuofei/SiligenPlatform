#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "sim/scheme_c/recorder.h"
#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/simulation_session.h"
#include "sim/scheme_c/tick_scheduler.h"
#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_clock.h"
#include "sim/scheme_c/virtual_io.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class CountingRuntimeBridge final : public sim::scheme_c::RuntimeBridge {
public:
    explicit CountingRuntimeBridge(sim::scheme_c::TickIndex complete_after_ticks)
        : complete_after_ticks_(complete_after_ticks) {}

    sim::scheme_c::RuntimeBridgeMetadata metadata() const override {
        sim::scheme_c::RuntimeBridgeMetadata metadata;
        metadata.owner = "packages/simulation-engine/runtime-bridge";
        metadata.next_integration_point =
            "Scheme C test bridge metadata mirrors the closed canonical runtime path.";
        return metadata;
    }

    const sim::scheme_c::RuntimeBridgeBindings& bindings() const override {
        return bindings_;
    }

    void attach(sim::scheme_c::VirtualAxisGroup&,
                sim::scheme_c::VirtualIo&,
                sim::scheme_c::Recorder* recorder) override {
        recorder_ = recorder;
    }

    void initialize(const sim::scheme_c::TickInfo& tick) override {
        initialized_tick_ = tick.tick_index;
    }

    bool advance(const sim::scheme_c::TickInfo& tick) override {
        advance_ticks_.push_back(tick.tick_index);
        if (recorder_ != nullptr) {
            recorder_->recordEvent(tick, "CountingRuntimeBridge", "advance");
        }

        ++advance_count_;
        return advance_count_ < complete_after_ticks_;
    }

    void requestStop() override {
        stop_requested_ = true;
    }

    const std::vector<sim::scheme_c::TickIndex>& advanceTicks() const {
        return advance_ticks_;
    }

    bool stopRequested() const {
        return stop_requested_;
    }

    sim::scheme_c::TickIndex initializedTick() const {
        return initialized_tick_;
    }

private:
    sim::scheme_c::RuntimeBridgeBindings bindings_{};
    sim::scheme_c::Recorder* recorder_{nullptr};
    sim::scheme_c::TickIndex complete_after_ticks_{1};
    sim::scheme_c::TickIndex advance_count_{0};
    sim::scheme_c::TickIndex initialized_tick_{0};
    std::vector<sim::scheme_c::TickIndex> advance_ticks_{};
    bool stop_requested_{false};
};

void testVirtualClockReportsStructuredErrors() {
    try {
        static_cast<void>(sim::scheme_c::VirtualClock(sim::scheme_c::Duration::zero()));
        throw std::runtime_error("virtual clock: expected invalid step error");
    } catch (const sim::scheme_c::VirtualTimeError& error) {
        require(error.code() == sim::scheme_c::VirtualTimeErrorCode::InvalidStep,
                "virtual clock: invalid step code mismatch");
    }

    sim::scheme_c::VirtualClock clock(std::chrono::milliseconds(1));
    clock.start(std::chrono::milliseconds(2));
    clock.pause();
    require(clock.state() == sim::scheme_c::VirtualClockState::Paused,
            "virtual clock: pause state mismatch");
    clock.resume();

    try {
        clock.start();
        throw std::runtime_error("virtual clock: expected duplicate start error");
    } catch (const sim::scheme_c::VirtualTimeError& error) {
        require(error.code() == sim::scheme_c::VirtualTimeErrorCode::AlreadyStarted,
                "virtual clock: duplicate start code mismatch");
    }

    clock.advance();
    require(clock.current().tick_index == 1, "virtual clock: expected one tick after resume");

    try {
        clock.advance();
        throw std::runtime_error("virtual clock: expected timeout error");
    } catch (const sim::scheme_c::VirtualTimeError& error) {
        require(error.code() == sim::scheme_c::VirtualTimeErrorCode::TimeoutReached,
                "virtual clock: timeout code mismatch");
        require(clock.state() == sim::scheme_c::VirtualClockState::TimedOut,
                "virtual clock: timeout state mismatch");
        require(error.tick().tick_index == 2, "virtual clock: timeout tick mismatch");
    }
}

void testTickSchedulerDispatchOrderIsDeterministic() {
    sim::scheme_c::TickScheduler scheduler;
    std::vector<std::string> order;

    scheduler.scheduleRecurring(
        sim::scheme_c::TickTaskPhase::Default,
        [&order, &scheduler](const sim::scheme_c::TickInfo&) {
            order.push_back("recurring");
            scheduler.scheduleOnceAt(
                0,
                sim::scheme_c::TickTaskPhase::Default,
                [&order](const sim::scheme_c::TickInfo&) { order.push_back("deferred"); },
                "deferred");
        },
        "recurring");
    scheduler.scheduleOnceAt(
        0,
        sim::scheme_c::TickTaskPhase::VirtualIo,
        [&order](const sim::scheme_c::TickInfo&) { order.push_back("io"); },
        "io");
    scheduler.scheduleOnceAt(
        0,
        sim::scheme_c::TickTaskPhase::RuntimeBridge,
        [&order](const sim::scheme_c::TickInfo&) { order.push_back("bridge-1"); },
        "bridge-1");
    scheduler.scheduleOnceAt(
        0,
        sim::scheme_c::TickTaskPhase::RuntimeBridge,
        [&order](const sim::scheme_c::TickInfo&) { order.push_back("bridge-2"); },
        "bridge-2");
    scheduler.scheduleOnceAt(
        0,
        sim::scheme_c::TickTaskPhase::Recording,
        [&order](const sim::scheme_c::TickInfo&) { order.push_back("record"); },
        "record");

    scheduler.dispatch(sim::scheme_c::TickInfo{0, std::chrono::milliseconds(0), std::chrono::milliseconds(1)});
    require(order.size() == 5U, "tick scheduler: unexpected first dispatch size");
    require(order[0] == "bridge-1", "tick scheduler: first runtime bridge task mismatch");
    require(order[1] == "bridge-2", "tick scheduler: second runtime bridge task mismatch");
    require(order[2] == "io", "tick scheduler: virtual io task mismatch");
    require(order[3] == "record", "tick scheduler: recording task mismatch");
    require(order[4] == "recurring", "tick scheduler: recurring task mismatch");

    scheduler.dispatch(sim::scheme_c::TickInfo{1, std::chrono::milliseconds(1), std::chrono::milliseconds(1)});
    require(order.size() == 7U, "tick scheduler: unexpected second dispatch size");
    require(order[5] == "deferred", "tick scheduler: deferred task should run on next dispatch");
    require(order[6] == "recurring", "tick scheduler: recurring task should continue running");
}

void testSimulationSessionSupportsPauseResumeAndStop() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X"};
    config.max_ticks = 10;

    auto runtime_bridge = std::make_unique<CountingRuntimeBridge>(50);
    auto* bridge_ptr = runtime_bridge.get();
    sim::scheme_c::SimulationSession session(
        config,
        std::move(runtime_bridge),
        sim::scheme_c::createInMemoryVirtualAxisGroup(config.axis_names),
        sim::scheme_c::createInMemoryVirtualIo(config.io_channels),
        sim::scheme_c::createTimelineRecorder());

    session.start();
    require(bridge_ptr->initializedTick() == 0U, "session: bridge initialize tick mismatch");
    require(session.status() == sim::scheme_c::SessionStatus::Running, "session: start status mismatch");
    require(session.advanceOneTick(), "session: first tick should advance");
    require(session.clock().current().tick_index == 1U, "session: first tick index mismatch");

    session.pause();
    require(session.status() == sim::scheme_c::SessionStatus::Paused, "session: pause status mismatch");
    require(!session.advanceOneTick(), "session: paused tick should not advance");
    require(session.clock().current().tick_index == 1U, "session: paused clock should not move");

    session.resume();
    require(session.status() == sim::scheme_c::SessionStatus::Running, "session: resume status mismatch");
    require(session.advanceOneTick(), "session: second tick should advance after resume");
    require(session.clock().current().tick_index == 2U, "session: second tick index mismatch");

    session.stop("manual stop for test");
    const auto result = session.finish();
    require(result.status == sim::scheme_c::SessionStatus::Stopped, "session: stop status mismatch");
    require(result.recording.ticks_executed == 2U, "session: stopped tick count mismatch");
    require(result.recording.summary.termination_reason == "application_requested_stop",
            "session: stopped termination reason mismatch");
    require(bridge_ptr->stopRequested(), "session: runtime bridge stop should be requested");
    require(bridge_ptr->advanceTicks() == std::vector<sim::scheme_c::TickIndex>{0, 1},
            "session: bridge advance ticks mismatch");
    require(std::any_of(
                result.recording.events.begin(),
                result.recording.events.end(),
                [](const sim::scheme_c::RecordedEvent& event) {
                    return event.message == "manual stop for test";
                }),
            "session: expected manual stop event");
}

void testSimulationSessionStopsAtTimeout() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X"};
    config.tick = std::chrono::milliseconds(1);
    config.timeout = std::chrono::milliseconds(2);
    config.max_ticks = 10;

    auto runtime_bridge = std::make_unique<CountingRuntimeBridge>(50);
    auto* bridge_ptr = runtime_bridge.get();
    sim::scheme_c::SimulationSession session(
        config,
        std::move(runtime_bridge),
        sim::scheme_c::createInMemoryVirtualAxisGroup(config.axis_names),
        sim::scheme_c::createInMemoryVirtualIo(config.io_channels),
        sim::scheme_c::createTimelineRecorder());

    const auto result = session.run();
    require(result.status == sim::scheme_c::SessionStatus::TimedOut, "session: timeout status mismatch");
    require(result.recording.ticks_executed == 2U, "session: timeout tick count mismatch");
    require(result.recording.simulated_time == std::chrono::milliseconds(2),
            "session: timeout simulated time mismatch");
    require(result.recording.snapshots.size() == 2U, "session: timeout snapshot count mismatch");
    require(result.recording.summary.termination_reason == "logical_timeout",
            "session: timeout termination reason mismatch");
    require(bridge_ptr->stopRequested(), "session: timeout should stop runtime bridge");
    require(bridge_ptr->advanceTicks() == std::vector<sim::scheme_c::TickIndex>{0, 1},
            "session: timeout bridge ticks mismatch");
}

}  // namespace

int main() {
    try {
        testVirtualClockReportsStructuredErrors();
        testTickSchedulerDispatchOrderIsDeterministic();
        testSimulationSessionSupportsPauseResumeAndStop();
        testSimulationSessionStopsAtTimeout();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
