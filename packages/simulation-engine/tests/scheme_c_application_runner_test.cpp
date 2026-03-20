#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

#include <nlohmann/json.hpp>

#include "sim/io/result_exporter.h"
#include "sim/scheme_c/application_runner.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path writeTempFile(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to create temporary canonical input: " + path.string());
    }

    output << content;
    return path;
}

std::filesystem::path findWorkspaceRoot() noexcept {
    std::error_code ec;
    auto current = std::filesystem::current_path(ec);
    if (ec) {
        return std::filesystem::path(".");
    }

    std::filesystem::path candidate = current;
    std::filesystem::path fallback = current;
    bool fallback_found = false;

    for (int level = 0; level < 10; ++level) {
        if (std::filesystem::exists(candidate / "WORKSPACE.md", ec) && !ec) {
            return candidate;
        }
        ec.clear();

        if (!fallback_found &&
            ((std::filesystem::exists(candidate / "CMakeLists.txt", ec) && !ec) ||
             (std::filesystem::exists(candidate / ".git", ec) && !ec))) {
            fallback = candidate;
            fallback_found = true;
        }
        ec.clear();

        auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return fallback_found ? fallback : current;
}

std::string resolveWorkspaceAssetPath(std::initializer_list<std::string> relative_candidates) {
    const auto workspace_root = findWorkspaceRoot();
    std::error_code ec;

    for (const auto& relative_candidate : relative_candidates) {
        const auto candidate = workspace_root / relative_candidate;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        ec.clear();
    }

    return (workspace_root / *relative_candidates.begin()).string();
}

bool nearlyEqual(double lhs, double rhs, double epsilon = 0.05) {
    return std::abs(lhs - rhs) <= epsilon;
}

double findAxisPosition(const sim::scheme_c::SimulationSessionResult& result, const std::string& axis_name) {
    for (const auto& axis : result.recording.final_axes) {
        if (axis.axis == axis_name) {
            return axis.position_mm;
        }
    }

    throw std::runtime_error("axis not found: " + axis_name);
}

bool hasEventContaining(const sim::scheme_c::RecordingResult& recording, const std::string& text) {
    return std::any_of(
        recording.events.begin(),
        recording.events.end(),
        [&text](const sim::scheme_c::RecordedEvent& event) {
            return event.message.find(text) != std::string::npos;
        });
}

std::size_t countEventsContaining(const sim::scheme_c::RecordingResult& recording, const std::string& text) {
    return static_cast<std::size_t>(std::count_if(
        recording.events.begin(),
        recording.events.end(),
        [&text](const sim::scheme_c::RecordedEvent& event) {
            return event.message.find(text) != std::string::npos;
        }));
}

std::size_t findTimelineMessageIndex(const sim::scheme_c::RecordingResult& recording, const std::string& text) {
    const auto it = std::find_if(
        recording.timeline.begin(),
        recording.timeline.end(),
        [&text](const sim::scheme_c::TimelineEntry& entry) {
            return entry.message.find(text) != std::string::npos;
        });
    if (it == recording.timeline.end()) {
        throw std::runtime_error("timeline message not found: " + text);
    }

    return static_cast<std::size_t>(std::distance(recording.timeline.begin(), it));
}

bool findIoValue(const sim::scheme_c::SimulationSessionResult& result, const std::string& channel) {
    for (const auto& io : result.recording.final_io) {
        if (io.channel == channel) {
            return io.value;
        }
    }

    throw std::runtime_error("io channel not found: " + channel);
}

bool hasFollowingErrorSample(const sim::scheme_c::RecordingResult& recording, const std::string& axis_name) {
    return std::any_of(
        recording.motion_profile.begin(),
        recording.motion_profile.end(),
        [&axis_name](const sim::scheme_c::MotionProfileSample& sample) {
            return sample.axis == axis_name && sample.following_error_active &&
                std::abs(sample.following_error_mm) > 0.0;
        });
}

bool hasTraceField(const sim::scheme_c::RecordingResult& recording,
                   const std::string& subject,
                   const std::string& field) {
    return std::any_of(
        recording.trace.begin(),
        recording.trace.end(),
        [&subject, &field](const sim::scheme_c::StateTraceEntry& entry) {
            return entry.subject == subject && entry.field == field;
        });
}

sim::scheme_c::CanonicalSimulationInput makeMixedBufferedInput() {
    sim::scheme_c::CanonicalSimulationInput input;
    input.axis_names = {"X", "Y"};
    input.tick = std::chrono::milliseconds(1);
    input.timeout = std::chrono::seconds(10);
    input.max_ticks = 12000;
    input.home_before_run = false;
    input.path.max_velocity_mm_per_s = 20.0;
    input.path.max_acceleration_mm_per_s2 = 200.0;
    input.path.sample_dt_s = 0.001;
    input.path.segments = {
        {sim::scheme_c::RuntimePathSegmentType::Line, 20.0, 0.0, 0.0, 0.0},
        {sim::scheme_c::RuntimePathSegmentType::ArcCounterClockwise, 20.0, 10.0, 15.0, 5.0},
        {sim::scheme_c::RuntimePathSegmentType::Line, 10.0, 10.0, 0.0, 0.0}
    };
    return input;
}

void testRunnerProducesDeterministicCanonicalResult() {
    const auto input_path = resolveWorkspaceAssetPath({
        "examples/simulation/sample_trajectory.json",
    });

    const auto result_a = sim::scheme_c::runMinimalClosedLoopFile(input_path);
    const auto result_b = sim::scheme_c::runMinimalClosedLoopFile(input_path);

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "runner: canonical file should complete, reason=" + result_a.recording.summary.termination_reason);
    require(!result_a.recording.motion_profile.empty(), "runner: motion_profile should not be empty");
    require(!result_a.recording.timeline.empty(), "runner: timeline should not be empty");
    require(result_a.recording.summary.terminal_state == "completed", "runner: summary terminal state mismatch");
    require(result_a.recording.summary.motion_sample_count == result_a.recording.motion_profile.size(),
            "runner: summary motion sample count mismatch");
    require(result_a.recording.summary.timeline_count == result_a.recording.timeline.size(),
            "runner: summary timeline count mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 280.0), "runner: final X position mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 140.0), "runner: final Y position mismatch");
    require(hasEventContaining(result_a.recording, "Armed 4 canonical compare triggers"),
            "runner: canonical replay triggers should be armed");
    require(hasEventContaining(result_a.recording, "Triggered compare output DO_VALVE=true"),
            "runner: canonical compare output should fire");
    require(hasEventContaining(result_a.recording, "Applied IO replay DO_VALVE=true"),
            "runner: canonical delayed IO should be recorded");
    require(hasEventContaining(result_a.recording, "Applied valve replay DO_VALVE=true."),
            "runner: canonical valve open delay should be recorded");
    require(hasEventContaining(result_a.recording, "Applied valve replay DO_VALVE=false."),
            "runner: canonical valve replay should be recorded");
    require(findTimelineMessageIndex(result_a.recording, "Armed 4 canonical compare triggers") <
                findTimelineMessageIndex(result_a.recording, "Triggered compare output DO_VALVE=true"),
            "runner: compare output should trigger after arming");
    require(findTimelineMessageIndex(result_a.recording, "Triggered compare output DO_VALVE=true") <
                findTimelineMessageIndex(result_a.recording, "Applied IO replay DO_VALVE=true"),
            "runner: delayed IO should replay after compare output");
    require(findTimelineMessageIndex(result_a.recording, "Applied IO replay DO_VALVE=true") <
                findTimelineMessageIndex(result_a.recording, "Applied valve replay DO_VALVE=true."),
            "runner: valve delay should replay after delayed IO");

    const auto json_a = sim::ResultExporter::toJson(result_a, false);
    const auto json_b = sim::ResultExporter::toJson(result_b, false);
    require(json_a == json_b, "runner: repeated execution should produce identical JSON");

    const auto json = nlohmann::json::parse(json_a);
    require(json.at("runtime_bridge").at("follow_up_seams").empty(),
            "runner: exported JSON should not keep stale follow_up_seams");
    require(json.at("recording").contains("motion_profile"), "runner: exported JSON missing motion_profile");
    require(json.at("recording").contains("timeline"), "runner: exported JSON missing timeline");
    require(json.at("recording").contains("summary"), "runner: exported JSON missing summary");
}

void testRunnerProducesDeterministicLongRectDiagResult() {
    const auto input_path = resolveWorkspaceAssetPath({
        "examples/simulation/rect_diag.simulation-input.json",
    });

    const auto result_a = sim::scheme_c::runMinimalClosedLoopFile(input_path);
    const auto result_b = sim::scheme_c::runMinimalClosedLoopFile(input_path);

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "runner: rect_diag should complete, reason=" + result_a.recording.summary.termination_reason);
    require(result_a.recording.summary.termination_reason == "bridge_completed",
            "runner: rect_diag termination mismatch");
    require(result_a.recording.summary.motion_sample_count > 6000U,
            "runner: rect_diag should generate a long deterministic motion profile");
    require(result_a.recording.summary.timeline_count >= result_a.recording.summary.event_count,
            "runner: rect_diag timeline should include all recorded events");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 100.0), "runner: rect_diag final X mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 100.0), "runner: rect_diag final Y mismatch");
    require(hasEventContaining(result_a.recording, "Session started."),
            "runner: rect_diag session start should be recorded");
    require(hasEventContaining(result_a.recording, "Delegated path command #"),
            "runner: rect_diag should use the core-owned deterministic path execution");

    const auto json_a = sim::ResultExporter::toJson(result_a, false);
    const auto json_b = sim::ResultExporter::toJson(result_b, false);
    require(json_a == json_b, "runner: rect_diag repeated execution should stay deterministic");
}

void testRunnerReturnsStructuredInvalidInput() {
    sim::scheme_c::CanonicalSimulationInput input;
    input.axis_names = {"X", "Y"};
    input.tick = std::chrono::milliseconds(1);
    input.timeout = std::chrono::milliseconds(100);
    input.max_ticks = 101;

    const auto result = sim::scheme_c::runMinimalClosedLoop(input);
    require(result.status == sim::scheme_c::SessionStatus::Failed, "runner: invalid input should fail");
    require(result.recording.summary.termination_reason == "invalid_input",
            "runner: invalid input termination mismatch");
    require(!result.recording.timeline.empty(), "runner: invalid input should emit structured timeline");
    require(hasEventContaining(result.recording, "path.segments must not be empty."),
            "runner: invalid input detail should be recorded");
}

void testRunnerReturnsStructuredMissingAxes() {
    auto input = sim::scheme_c::loadCanonicalSimulationInputFile(resolveWorkspaceAssetPath({
        "examples/simulation/sample_trajectory.json",
    }));
    input.axis_names = {"X"};

    const auto result = sim::scheme_c::runMinimalClosedLoop(input);
    require(result.status == sim::scheme_c::SessionStatus::Failed, "runner: missing axes should fail");
    require(result.recording.summary.termination_reason == "missing_required_axes",
            "runner: missing axes termination mismatch");
    require(hasEventContaining(result.recording, "axis_names must contain X and Y."),
            "runner: missing axes detail should be recorded");
}

void testRunnerReturnsStructuredTimeout() {
    auto input = sim::scheme_c::loadCanonicalSimulationInputFile(resolveWorkspaceAssetPath({
        "examples/simulation/sample_trajectory.json",
    }));
    input.home_before_run = false;
    input.timeout = std::chrono::milliseconds(5);
    input.max_ticks = 100;

    const auto result = sim::scheme_c::runMinimalClosedLoop(input);
    require(result.status == sim::scheme_c::SessionStatus::TimedOut, "runner: timeout should return timed_out");
    require(result.recording.summary.termination_reason == "logical_timeout",
            "runner: timeout termination mismatch");
    require(hasEventContaining(result.recording, "Stopped after reaching logical timeout."),
            "runner: timeout event should be recorded");
}

void testRunnerRecordsStopDuringExecution() {
    auto input = sim::scheme_c::loadCanonicalSimulationInputFile(resolveWorkspaceAssetPath({
        "examples/simulation/sample_trajectory.json",
    }));
    input.home_before_run = false;
    input.path.max_velocity_mm_per_s = 20.0;
    input.timeout = std::chrono::seconds(5);
    input.max_ticks = 6000;

    sim::scheme_c::StopDirective stop;
    stop.after_tick = 5U;
    stop.immediate = true;
    stop.clear_pending_commands = true;
    input.stop = stop;

    const auto result = sim::scheme_c::runMinimalClosedLoop(input);
    require(result.status == sim::scheme_c::SessionStatus::Completed,
            "runner: stop should still drain to completed, reason=" + result.recording.summary.termination_reason);
    require(result.recording.summary.termination_reason == "bridge_completed",
            "runner: stop path should drain through bridge completion");
    require(hasEventContaining(result.recording, "Submitted stop command #"),
            "runner: stop submission event should be recorded");
    require(!nearlyEqual(findAxisPosition(result, "X"), 280.0) ||
                !nearlyEqual(findAxisPosition(result, "Y"), 140.0),
            "runner: stop should prevent reaching the final path endpoint");
}

void testRunnerKeepsMixedBufferedPathDeterministic() {
    const auto input = makeMixedBufferedInput();

    const auto result_a = sim::scheme_c::runMinimalClosedLoop(input);
    const auto result_b = sim::scheme_c::runMinimalClosedLoop(input);

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "runner: mixed buffered input should complete");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 10.0), "runner: mixed buffered final X mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 10.0), "runner: mixed buffered final Y mismatch");
    require(result_a.recording.summary.termination_reason == "bridge_completed",
            "runner: mixed buffered termination mismatch");

    const auto json_a = sim::ResultExporter::toJson(result_a, false);
    const auto json_b = sim::ResultExporter::toJson(result_b, false);
    require(json_a == json_b, "runner: mixed buffered repeated execution should stay deterministic");
}

void testRunnerSupportsDeterministicMotionRealismEnhancement() {
    const auto input = sim::scheme_c::loadCanonicalSimulationInputJson(
        R"({
  "timestep_seconds": 1.0,
  "max_simulation_time_seconds": 40.0,
  "max_velocity": 12.0,
  "max_acceleration": 24.0,
  "home_before_run": false,
  "motion_realism": {
    "enabled": true,
    "servo_lag_seconds": 4.0,
    "encoder_quantization_mm": 0.5
  },
  "segments": [
    {
      "type": "LINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 12.0, "y": 0.0 }
    }
  ]
})",
        "inline_motion_realism");

    const auto result_a = sim::scheme_c::runMinimalClosedLoop(input);
    const auto result_b = sim::scheme_c::runMinimalClosedLoop(input);

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "runner realism: realism-enhanced input should complete");
    require(nearlyEqual(findAxisPosition(result_a, "X"), 12.0), "runner realism: final X mismatch");
    require(nearlyEqual(findAxisPosition(result_a, "Y"), 0.0), "runner realism: final Y mismatch");
    require(result_a.recording.summary.max_following_error_mm > 0.0,
            "runner realism: summary should report non-zero following error");
    require(result_a.recording.summary.following_error_sample_count > 0U,
            "runner realism: summary should count following error samples");
    require(hasFollowingErrorSample(result_a.recording, "X"),
            "runner realism: motion profile should contain following error samples");
    require(hasTraceField(result_a.recording, "X", "following_error_active"),
            "runner realism: trace should expose following error transitions");

    const auto json_a = sim::ResultExporter::toJson(result_a, false);
    const auto json_b = sim::ResultExporter::toJson(result_b, false);
    require(json_a == json_b, "runner realism: repeated execution should remain deterministic");

    const auto json = nlohmann::json::parse(json_a);
    require(json.at("recording").at("summary").at("max_following_error_mm").get<double>() > 0.0,
            "runner realism: exported summary should include motion error metrics");
    require(json.at("recording").at("summary").at("following_error_sample_count").get<std::size_t>() > 0U,
            "runner realism: exported summary should include following_error_sample_count");
}

void testRunnerReplaysCanonicalIoDelayWithoutValve() {
    const auto input = sim::scheme_c::loadCanonicalSimulationInputJson(
        R"({
  "timestep_seconds": 1.0,
  "max_simulation_time_seconds": 30.0,
  "max_velocity": 10.0,
  "max_acceleration": 20.0,
  "home_before_run": false,
  "segments": [
    {
      "type": "LINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 30.0, "y": 0.0 }
    }
  ],
  "io_delays": [
    { "channel": "DO_CAMERA_TRIGGER", "delay_seconds": 2.0 }
  ],
  "triggers": [
    { "channel": "DO_CAMERA_TRIGGER", "trigger_position": 10.0, "state": true }
  ]
})",
        "inline_camera_delay");

    const auto result = sim::scheme_c::runMinimalClosedLoop(input);

    require(result.status == sim::scheme_c::SessionStatus::Completed,
            "runner: camera replay input should complete");
    require(findIoValue(result, "DO_CAMERA_TRIGGER"),
            "runner: camera trigger should remain asserted after replay");
    require(hasEventContaining(result.recording, "Triggered compare output DO_CAMERA_TRIGGER=true"),
            "runner: camera compare output should fire");
    require(hasEventContaining(result.recording, "Applied IO replay DO_CAMERA_TRIGGER=true"),
            "runner: camera io delay should replay through runtime bridge");
    require(!hasEventContaining(result.recording, "Applied valve replay DO_CAMERA_TRIGGER"),
            "runner: camera replay should not route through valve delays");
}

void testRunnerReplaysCanonicalValveDelayFromInput() {
    const auto input = sim::scheme_c::loadCanonicalSimulationInputJson(
        R"({
  "timestep_seconds": 1.0,
  "max_simulation_time_seconds": 30.0,
  "max_velocity": 10.0,
  "max_acceleration": 20.0,
  "home_before_run": false,
  "segments": [
    {
      "type": "LINE",
      "start": { "x": 0.0, "y": 0.0 },
      "end": { "x": 40.0, "y": 0.0 }
    }
  ],
  "io_delays": [
    { "channel": "DO_VALVE", "delay_seconds": 1.0 }
  ],
  "triggers": [
    { "channel": "DO_VALVE", "trigger_position": 10.0, "state": true },
    { "channel": "DO_VALVE", "trigger_position": 20.0, "state": false }
  ],
  "valve": {
    "open_delay_seconds": 1.0,
    "close_delay_seconds": 2.0
  }
})",
        "inline_valve_delay");

    const auto result_a = sim::scheme_c::runMinimalClosedLoop(input);
    const auto result_b = sim::scheme_c::runMinimalClosedLoop(input);

    require(result_a.status == sim::scheme_c::SessionStatus::Completed,
            "runner: valve replay input should complete");
    require(!findIoValue(result_a, "DO_VALVE"),
            "runner: valve should end closed after replay");
    require(hasEventContaining(result_a.recording, "Applied IO replay DO_VALVE=true"),
            "runner: valve open command should replay after io delay");
    require(hasEventContaining(result_a.recording, "Applied valve replay DO_VALVE=true."),
            "runner: valve open delay should be recorded");
    require(hasEventContaining(result_a.recording, "Applied valve replay DO_VALVE=false."),
            "runner: valve close delay should be recorded");
    require(result_a.recording.summary.timeline_count >= result_a.recording.summary.event_count,
            "runner: replayed valve events should be visible in timeline");

    const auto json_a = sim::ResultExporter::toJson(result_a, false);
    const auto json_b = sim::ResultExporter::toJson(result_b, false);
    require(json_a == json_b, "runner: valve replay should remain deterministic across runs");
}

void testRunnerReturnsStructuredFileValidationFailure() {
    const auto path = writeTempFile(
        "simulation_engine_scheme_c_invalid_input.json",
        R"({
  "timestep_seconds": 0.001,
  "max_simulation_time_seconds": 30.0,
  "max_velocity": 10.0,
  "max_acceleration": 20.0
})");

    const auto result = sim::scheme_c::runMinimalClosedLoopFile(path.string());
    std::filesystem::remove(path);

    require(result.status == sim::scheme_c::SessionStatus::Failed,
            "runner: invalid canonical file should fail");
    require(result.recording.summary.termination_reason == "invalid_input",
            "runner: invalid canonical file termination mismatch");
    require(result.recording.summary.timeline_count == 1U,
            "runner: invalid canonical file should produce a single structured event");
    require(hasEventContaining(result.recording, "segments must be a non-empty array."),
            "runner: invalid canonical file detail should be recorded");
}

}  // namespace

int main() {
    try {
        testRunnerProducesDeterministicCanonicalResult();
        testRunnerProducesDeterministicLongRectDiagResult();
        testRunnerReturnsStructuredInvalidInput();
        testRunnerReturnsStructuredMissingAxes();
        testRunnerReturnsStructuredTimeout();
        testRunnerRecordsStopDuringExecution();
        testRunnerKeepsMixedBufferedPathDeterministic();
        testRunnerSupportsDeterministicMotionRealismEnhancement();
        testRunnerReplaysCanonicalIoDelayWithoutValve();
        testRunnerReplaysCanonicalValveDelayFromInput();
        testRunnerReturnsStructuredFileValidationFailure();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
