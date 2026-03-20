#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/simulation_session.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

double findAxisPosition(const sim::scheme_c::SimulationSessionResult& result, const std::string& axis_name) {
    for (const auto& axis : result.recording.final_axes) {
        if (axis.axis == axis_name) {
            return axis.position_mm;
        }
    }

    throw std::runtime_error("axis not found: " + axis_name);
}

bool nearlyEqual(double lhs, double rhs, double epsilon = 0.01) {
    return std::abs(lhs - rhs) <= epsilon;
}

std::pair<double, double> currentPosition(sim::scheme_c::SimulationSession& session) {
    const auto x = session.axisGroup().readAxis("X").position_mm;
    const auto y = session.axisGroup().readAxis("Y").position_mm;
    return {x, y};
}

bool hasEventContaining(const sim::scheme_c::SimulationSessionResult& result, const std::string& text) {
    return std::any_of(
        result.recording.events.begin(),
        result.recording.events.end(),
        [&text](const sim::scheme_c::RecordedEvent& event) {
            return event.message.find(text) != std::string::npos;
        });
}

std::size_t countEventsContaining(const sim::scheme_c::SimulationSessionResult& result, const std::string& text) {
    return static_cast<std::size_t>(std::count_if(
        result.recording.events.begin(),
        result.recording.events.end(),
        [&text](const sim::scheme_c::RecordedEvent& event) {
            return event.message.find(text) != std::string::npos;
        }));
}

std::string resultSignature(const sim::scheme_c::SimulationSessionResult& result) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(9);
    stream << static_cast<int>(result.status) << '|'
           << result.recording.summary.terminal_state << '|'
           << result.recording.summary.termination_reason << '|'
           << result.recording.summary.ticks_executed << '|'
           << result.recording.summary.simulated_time.count() << '|'
           << result.recording.summary.motion_sample_count << '|'
           << result.recording.summary.timeline_count << '|'
           << result.recording.summary.event_count << '|'
           << result.recording.summary.trace_count << '\n';

    for (const auto& axis : result.recording.final_axes) {
        stream << "axis|" << axis.axis << '|'
               << axis.position_mm << '|'
               << axis.velocity_mm_per_s << '|'
               << axis.running << '|'
               << axis.done << '|'
               << axis.homed << '|'
               << axis.has_error << '|'
               << axis.error_code << '\n';
    }

    for (const auto& io : result.recording.final_io) {
        stream << "io|" << io.channel << '|'
               << io.value << '|'
               << io.output << '\n';
    }

    for (const auto& event : result.recording.events) {
        stream << "event|" << event.tick.tick_index << '|'
               << event.source << '|'
               << event.message << '\n';
    }

    for (const auto& entry : result.recording.timeline) {
        stream << "timeline|" << entry.tick.tick_index << '|'
               << entry.kind << '|'
               << entry.source << '|'
               << entry.subject << '|'
               << entry.field << '|'
               << entry.previous_value << '|'
               << entry.current_value << '|'
               << entry.message << '\n';
    }

    return stream.str();
}

bool findIoValue(const sim::scheme_c::SimulationSessionResult& result, const std::string& channel) {
    for (const auto& io : result.recording.final_io) {
        if (io.channel == channel) {
            return io.value;
        }
    }

    throw std::runtime_error("io channel not found: " + channel);
}

bool currentIoValue(sim::scheme_c::SimulationSession& session, const std::string& channel) {
    return session.io().readSignal(channel);
}

std::string commandStateDetail(const sim::scheme_c::RuntimeBridgeControl& control, std::uint64_t command_id) {
    const auto status = control.commandStatus(command_id);
    if (!status.has_value()) {
        return "missing";
    }

    return std::to_string(static_cast<int>(status->state)) + ":" + status->detail;
}

sim::scheme_c::SimulationSessionConfig makeReplayHeavyConfig() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y"};
    config.io_channels = {"DO_VALVE", "DO_CAMERA_TRIGGER"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 20000;
    config.replay_plan.io_delays = {
        {"DO_VALVE", std::chrono::milliseconds(3)},
        {"DO_CAMERA_TRIGGER", std::chrono::milliseconds(1)}
    };
    config.replay_plan.triggers = {
        {"DO_VALVE", 40.0, true, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0},
        {"DO_CAMERA_TRIGGER", 120.0, true, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0},
        {"DO_VALVE", 180.0, false, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0},
        {"DO_VALVE", 240.0, true, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0},
        {"DO_CAMERA_TRIGGER", 300.0, false, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0},
        {"DO_VALVE", 360.0, false, 0, sim::scheme_c::ReplayTriggerSpace::PathProgress, 0}
    };
    config.replay_plan.valve = sim::scheme_c::ReplayValveSpec{
        "DO_VALVE",
        std::chrono::milliseconds(2),
        std::chrono::milliseconds(4)
    };
    return config;
}

sim::scheme_c::RuntimeBridgePathCommand makeLongMixedPathCommand() {
    sim::scheme_c::RuntimeBridgePathCommand path_command;
    path_command.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 40.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 80.0, 20.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 120.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::ArcCounterClockwise, 160.0, 40.0, 120.0, 40.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 200.0, 40.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::ArcClockwise, 240.0, 0.0, 200.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 280.0, 20.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 320.0, 20.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 360.0, 0.0, 0.0, 0.0}
    };
    path_command.max_velocity_mm_per_s = 60.0;
    path_command.max_acceleration_mm_per_s2 = 400.0;
    path_command.sample_dt_s = 0.001;
    return path_command;
}

sim::scheme_c::SimulationSessionResult runLongPathWithPauseSchedule(const std::vector<int>& pause_ticks) {
    auto session = sim::scheme_c::createBaselineSession(makeReplayHeavyConfig());
    auto* control = sim::scheme_c::runtimeBridgeControl(session.runtimeBridge());
    require(control != nullptr, "pause schedule: runtime bridge control should be available");

    const auto path_id = control->submitPath(makeLongMixedPathCommand());
    session.start();

    std::size_t next_pause_index = 0;
    while (session.status() == sim::scheme_c::SessionStatus::Running) {
        if (!session.advanceOneTick()) {
            break;
        }

        if (next_pause_index >= pause_ticks.size()) {
            continue;
        }

        if (session.clock().current().tick_index < static_cast<sim::scheme_c::TickIndex>(pause_ticks[next_pause_index])) {
            continue;
        }

        const auto before_pause = currentPosition(session);
        const auto valve_before_pause = currentIoValue(session, "DO_VALVE");
        const auto camera_before_pause = currentIoValue(session, "DO_CAMERA_TRIGGER");
        const auto path_status = control->commandStatus(path_id);
        require(path_status.has_value() &&
                    path_status->state == sim::scheme_c::RuntimeBridgeCommandState::Active,
                "pause schedule: path should remain active across pause cycles");

        session.pause();
        require(!session.advanceOneTick(), "pause schedule: paused session should not advance");
        const auto during_pause = currentPosition(session);
        require(
            nearlyEqual(before_pause.first, during_pause.first) &&
                nearlyEqual(before_pause.second, during_pause.second),
            "pause schedule: axis position should remain stable while paused");
        require(currentIoValue(session, "DO_VALVE") == valve_before_pause,
                "pause schedule: valve IO should remain stable while paused");
        require(currentIoValue(session, "DO_CAMERA_TRIGGER") == camera_before_pause,
                "pause schedule: camera IO should remain stable while paused");

        session.resume();
        ++next_pause_index;
    }

    const auto result = session.finish();
    const auto path_status = control->commandStatus(path_id);
    require(path_status.has_value() &&
                path_status->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "pause schedule: path should complete after the final resume");
    return result;
}

void testRuntimeBridgeRunsHomeMoveAndPathOnProcessRuntimeCore() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y"};
    config.io_channels = {"DO_VALVE", "DO_CAMERA_TRIGGER"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 5000;
    config.replay_plan.io_delays.push_back({"DO_CAMERA_TRIGGER", std::chrono::milliseconds(1)});
    config.replay_plan.triggers.push_back({
        "DO_CAMERA_TRIGGER",
        0.02,
        true,
        0,
        sim::scheme_c::ReplayTriggerSpace::PathProgress,
        0
    });

    auto session = sim::scheme_c::createBaselineSession(config);
    auto* control = sim::scheme_c::runtimeBridgeControl(session.runtimeBridge());
    require(control != nullptr, "runtime bridge control should be available");

    const auto home_id = control->submitHome(sim::scheme_c::RuntimeBridgeHomeCommand{});
    const auto move_id = control->submitMove(sim::scheme_c::RuntimeBridgeMoveCommand{
        10.0,
        0.0,
        20.0
    });

    sim::scheme_c::RuntimeBridgePathCommand path_command;
    path_command.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 20.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::ArcCounterClockwise, 20.0, 10.0, 15.0, 5.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 10.0, 10.0, 0.0, 0.0}
    };
    path_command.max_velocity_mm_per_s = 20.0;
    path_command.max_acceleration_mm_per_s2 = 200.0;
    path_command.sample_dt_s = 0.001;

    const auto path_id = control->submitPath(path_command);
    const auto result = session.run();
    const auto home_detail = commandStateDetail(*control, home_id);
    const auto move_detail = commandStateDetail(*control, move_id);
    const auto path_detail = commandStateDetail(*control, path_id);

    require(
        result.status == sim::scheme_c::SessionStatus::Completed,
        "bridge run should complete, status=" + std::to_string(static_cast<int>(result.status)) +
            ", reason=" + result.recording.summary.termination_reason +
            ", home=" + home_detail +
            ", move=" + move_detail +
            ", path=" + path_detail);
    require(result.runtime_bridge.mode == sim::scheme_c::RuntimeBridgeMode::ProcessRuntimeCore,
            "runtime bridge should report process-runtime-core mode");
    require(
        result.runtime_bridge.next_integration_point.find("SC-B-002 closed") != std::string::npos,
        "runtime bridge should report the closed core-owned assembly seam");
    require(!result.recording.events.empty(), "recording events should not be empty");
    require(!result.recording.timeline.empty(), "recording timeline should not be empty");
    require(
        hasEventContaining(result, "Delegated path command #"),
        "path command should be delegated to core-owned deterministic path execution");
    require(
        hasEventContaining(result, "Armed compare output DO_CAMERA_TRIGGER=true"),
        "compare output should be armed through the trigger controller adapter");
    require(
        hasEventContaining(result, "Triggered compare output DO_CAMERA_TRIGGER=true"),
        "compare output should fire on deterministic replay progress");
    require(findIoValue(result, "DO_CAMERA_TRIGGER"),
            "compare output replay should drive the virtual IO timeline");
    require(nearlyEqual(findAxisPosition(result, "X"), 10.0), "final X position mismatch");
    require(nearlyEqual(findAxisPosition(result, "Y"), 10.0), "final Y position mismatch");

    const auto home_status = control->commandStatus(home_id);
    const auto move_status = control->commandStatus(move_id);
    const auto path_status = control->commandStatus(path_id);

    require(home_status.has_value() &&
                home_status->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "home command should complete");
    require(move_status.has_value() &&
                move_status->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "move command should complete");
    require(path_status.has_value() &&
                path_status->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "path command should complete");
}

void testRuntimeBridgeLongContinuousMixedPathStaysDeterministic() {
    auto session_a = sim::scheme_c::createBaselineSession(makeReplayHeavyConfig());
    auto session_b = sim::scheme_c::createBaselineSession(makeReplayHeavyConfig());
    auto* control_a = sim::scheme_c::runtimeBridgeControl(session_a.runtimeBridge());
    auto* control_b = sim::scheme_c::runtimeBridgeControl(session_b.runtimeBridge());
    require(control_a != nullptr && control_b != nullptr,
            "long path: runtime bridge control should be available");

    const auto path_id_a = control_a->submitPath(makeLongMixedPathCommand());
    const auto path_id_b = control_b->submitPath(makeLongMixedPathCommand());

    const auto result_a = session_a.run();
    const auto result_b = session_b.run();

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "long path: first run should complete");
    require(result_b.status == sim::scheme_c::SessionStatus::Completed,
            "long path: second run should complete");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 360.0), "long path: final X mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 0.0), "long path: final Y mismatch");
    require(!findIoValue(result_a, "DO_VALVE"), "long path: valve should end closed");
    require(!findIoValue(result_a, "DO_CAMERA_TRIGGER"), "long path: camera output should end low");
    require(result_a.recording.summary.motion_sample_count > 10000U,
            "long path: expected a long multi-segment recording");
    require(hasEventContaining(result_a, "Triggered compare output DO_CAMERA_TRIGGER=true"),
            "long path: camera compare output should fire");
    require(hasEventContaining(result_a, "Applied IO replay DO_CAMERA_TRIGGER=true"),
            "long path: delayed camera IO should replay");
    require(hasEventContaining(result_a, "Applied valve replay DO_VALVE=true."),
            "long path: valve open delay should be recorded");
    require(hasEventContaining(result_a, "Applied valve replay DO_VALVE=false."),
            "long path: valve close delay should be recorded");

    const auto status_a = control_a->commandStatus(path_id_a);
    const auto status_b = control_b->commandStatus(path_id_b);
    require(status_a.has_value() &&
                status_a->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "long path: first path command should complete");
    require(status_b.has_value() &&
                status_b->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "long path: second path command should complete");

    require(resultSignature(result_a) == resultSignature(result_b),
            "long path: repeated execution should remain deterministic");
}

void testRuntimeBridgeStopPreemptsActivePath() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y"};
    config.io_channels = {"DO_VALVE"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 2000;
    config.replay_plan.io_delays.push_back({"DO_VALVE", std::chrono::milliseconds(10)});
    config.replay_plan.triggers.push_back({
        "DO_VALVE",
        0.02,
        true,
        0,
        sim::scheme_c::ReplayTriggerSpace::PathProgress,
        0
    });
    config.replay_plan.valve = sim::scheme_c::ReplayValveSpec{
        "DO_VALVE",
        std::chrono::milliseconds(5),
        std::chrono::milliseconds(5)
    };

    auto session = sim::scheme_c::createBaselineSession(config);
    auto* control = sim::scheme_c::runtimeBridgeControl(session.runtimeBridge());
    require(control != nullptr, "runtime bridge control should be available");

    sim::scheme_c::RuntimeBridgePathCommand path_command;
    path_command.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 50.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 50.0, 50.0, 0.0, 0.0}
    };
    path_command.max_velocity_mm_per_s = 10.0;
    path_command.max_acceleration_mm_per_s2 = 200.0;
    path_command.sample_dt_s = 0.001;

    const auto path_id = control->submitPath(path_command);

    session.start();

    std::uint64_t stop_id = 0;
    session.scheduler().scheduleOnceAt(
        5,
        sim::scheme_c::TickTaskPhase::Default,
        [&stop_id, control](const sim::scheme_c::TickInfo&) {
            stop_id = control->submitStop(sim::scheme_c::RuntimeBridgeStopCommand{true, true});
        },
        "stop_active_path");

    const auto result = session.run();

    require(stop_id != 0, "stop command should be submitted");
    const auto path_detail = commandStateDetail(*control, path_id);
    const auto stop_detail = commandStateDetail(*control, stop_id);
    require(
        result.status == sim::scheme_c::SessionStatus::Completed,
        "bridge run should complete after stop, status=" +
            std::to_string(static_cast<int>(result.status)) +
            ", reason=" + result.recording.summary.termination_reason +
            ", path=" + path_detail +
            ", stop=" + stop_detail);

    const auto final_x = findAxisPosition(result, "X");
    const auto final_y = findAxisPosition(result, "Y");
    require(!nearlyEqual(final_x, 50.0), "stop should prevent the path from reaching final X");
    require(!nearlyEqual(final_y, 50.0), "stop should prevent the path from reaching final Y");

    const auto path_status = control->commandStatus(path_id);
    const auto stop_status = control->commandStatus(stop_id);
    require(path_status.has_value() &&
                path_status->state == sim::scheme_c::RuntimeBridgeCommandState::Cancelled,
            "active path should be cancelled by stop");
    require(stop_status.has_value() &&
                stop_status->state == sim::scheme_c::RuntimeBridgeCommandState::Completed,
            "stop command should complete");
    require(!findIoValue(result, "DO_VALVE"),
            "stop should clear pending replay before delayed IO reaches virtual output");
    require(!hasEventContaining(result, "Applied IO replay DO_VALVE=true"),
            "stop should prevent delayed IO replay from executing");
}

void testRuntimeBridgePauseResumePreservesBufferedPathState() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y"};
    config.io_channels = {"DO_VALVE"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 10000;
    config.replay_plan.io_delays.push_back({"DO_VALVE", std::chrono::milliseconds(3)});
    config.replay_plan.triggers.push_back({
        "DO_VALVE",
        0.02,
        true,
        0,
        sim::scheme_c::ReplayTriggerSpace::PathProgress,
        0
    });
    config.replay_plan.valve = sim::scheme_c::ReplayValveSpec{
        "DO_VALVE",
        std::chrono::milliseconds(2),
        std::chrono::milliseconds(2)
    };

    auto session = sim::scheme_c::createBaselineSession(config);
    auto* control = sim::scheme_c::runtimeBridgeControl(session.runtimeBridge());
    require(control != nullptr, "runtime bridge control should be available");

    sim::scheme_c::RuntimeBridgePathCommand path_command;
    path_command.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 10.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 10.0, 10.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 0.0, 10.0, 0.0, 0.0}
    };
    path_command.max_velocity_mm_per_s = 10.0;
    path_command.max_acceleration_mm_per_s2 = 200.0;
    path_command.sample_dt_s = 0.001;

    const auto path_id = control->submitPath(path_command);

    session.start();
    for (int tick = 0; tick < 5; ++tick) {
        require(session.advanceOneTick(), "pause/resume: expected pre-pause tick to advance");
    }

    const auto position_before_pause = currentPosition(session);
    const auto pre_pause_status = control->commandStatus(path_id);
    require(pre_pause_status.has_value() &&
                pre_pause_status->state == sim::scheme_c::RuntimeBridgeCommandState::Active,
            "pause/resume: path should be active before pause");

    session.pause();
    require(session.status() == sim::scheme_c::SessionStatus::Paused, "pause/resume: session should be paused");
    require(!session.advanceOneTick(), "pause/resume: paused session should not advance");

    const auto position_during_pause = currentPosition(session);
    require(
        nearlyEqual(position_before_pause.first, position_during_pause.first) &&
            nearlyEqual(position_before_pause.second, position_during_pause.second),
        "pause/resume: axis position should remain stable while paused");
    require(!currentIoValue(session, "DO_VALVE"),
            "pause/resume: delayed replay should not apply while paused");

    const auto paused_status = control->commandStatus(path_id);
    require(paused_status.has_value() &&
                paused_status->state == sim::scheme_c::RuntimeBridgeCommandState::Active,
            "pause/resume: path should remain active while paused");

    session.resume();
    const auto result = session.run();

    require(result.status == sim::scheme_c::SessionStatus::Completed,
            "pause/resume: run should complete after resume");
    require(nearlyEqual(findAxisPosition(result, "X"), 0.0), "pause/resume: final X mismatch");
    require(nearlyEqual(findAxisPosition(result, "Y"), 10.0), "pause/resume: final Y mismatch");
    require(findIoValue(result, "DO_VALVE"),
            "pause/resume: replay should continue once the session resumes");
    require(hasEventContaining(result, "Applied valve replay DO_VALVE=true."),
            "pause/resume: resumed replay should reach the valve timeline");
}

void testRuntimeBridgeMultiplePauseResumeCyclesRemainDeterministic() {
    const auto result_a = runLongPathWithPauseSchedule({5, 25, 75});
    const auto result_b = runLongPathWithPauseSchedule({5, 25, 75});

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "multi pause/resume: first run should complete");
    require(result_b.status == sim::scheme_c::SessionStatus::Completed,
            "multi pause/resume: second run should complete");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 360.0),
            "multi pause/resume: final X mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 0.0),
            "multi pause/resume: final Y mismatch");
    require(countEventsContaining(result_a, "Session paused.") == 3U,
            "multi pause/resume: expected three pause events");
    require(countEventsContaining(result_a, "Session resumed.") == 3U,
            "multi pause/resume: expected three resume events");
    require(!findIoValue(result_a, "DO_VALVE"),
            "multi pause/resume: valve should end closed");
    require(!findIoValue(result_a, "DO_CAMERA_TRIGGER"),
            "multi pause/resume: camera output should end low");

    require(resultSignature(result_a) == resultSignature(result_b),
            "multi pause/resume: repeated pause schedule should stay deterministic");
}

void testRuntimeBridgeSessionStopCancelsBufferedPath() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X", "Y"};
    config.io_channels = {"DO_CAMERA_TRIGGER"};
    config.tick = std::chrono::milliseconds(1);
    config.max_ticks = 4000;
    config.replay_plan.io_delays.push_back({"DO_CAMERA_TRIGGER", std::chrono::milliseconds(10)});
    config.replay_plan.triggers.push_back({
        "DO_CAMERA_TRIGGER",
        0.02,
        true,
        0,
        sim::scheme_c::ReplayTriggerSpace::PathProgress,
        0
    });

    auto session = sim::scheme_c::createBaselineSession(config);
    auto* control = sim::scheme_c::runtimeBridgeControl(session.runtimeBridge());
    require(control != nullptr, "runtime bridge control should be available");

    sim::scheme_c::RuntimeBridgePathCommand path_command;
    path_command.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 20.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 20.0, 20.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 0.0, 20.0, 0.0, 0.0}
    };
    path_command.max_velocity_mm_per_s = 10.0;
    path_command.max_acceleration_mm_per_s2 = 200.0;
    path_command.sample_dt_s = 0.001;

    const auto path_id = control->submitPath(path_command);

    session.start();
    for (int tick = 0; tick < 5; ++tick) {
        require(session.advanceOneTick(), "session stop: expected tick to advance before stop");
    }

    session.stop("session stop for buffered path");
    const auto result = session.finish();

    require(result.status == sim::scheme_c::SessionStatus::Stopped,
            "session stop: finish should preserve stopped state");
    const auto path_status = control->commandStatus(path_id);
    require(path_status.has_value() &&
                path_status->state == sim::scheme_c::RuntimeBridgeCommandState::Cancelled,
            "session stop: path should be cancelled by session stop");
    require(!control->hasPendingCommands(), "session stop: buffered work should be cleared");
    require(!nearlyEqual(findAxisPosition(result, "X"), 0.0) ||
                !nearlyEqual(findAxisPosition(result, "Y"), 20.0),
            "session stop: cancelled path should not reach final endpoint");
    require(!findIoValue(result, "DO_CAMERA_TRIGGER"),
            "session stop: pending replay should not be applied after stop");
    require(!hasEventContaining(result, "Applied IO replay DO_CAMERA_TRIGGER=true"),
            "session stop: replay events should be cleared together with path state");
}

}  // namespace

int main() {
    try {
        testRuntimeBridgeRunsHomeMoveAndPathOnProcessRuntimeCore();
        testRuntimeBridgeLongContinuousMixedPathStaysDeterministic();
        testRuntimeBridgeStopPreemptsActivePath();
        testRuntimeBridgePauseResumePreservesBufferedPathState();
        testRuntimeBridgeMultiplePauseResumeCyclesRemainDeterministic();
        testRuntimeBridgeSessionStopCancelsBufferedPath();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
