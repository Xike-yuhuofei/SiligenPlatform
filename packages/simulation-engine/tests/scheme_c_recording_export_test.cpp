#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "sim/io/result_exporter.h"
#include "sim/scheme_c/recorder.h"
#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/simulation_session.h"
#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_io.h"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void testRecorderBuildsTraceTimelineAndSummary() {
    auto recorder = sim::scheme_c::createTimelineRecorder();

    const sim::scheme_c::TickInfo first_tick{0U, std::chrono::milliseconds(0), std::chrono::milliseconds(1)};
    const sim::scheme_c::TickInfo second_tick{1U, std::chrono::milliseconds(1), std::chrono::milliseconds(1)};

    recorder->recordEvent(first_tick, "RuntimeBridge", "initialized");
    recorder->recordSnapshot(
        first_tick,
        {sim::scheme_c::AxisState{"X"}, sim::scheme_c::AxisState{"Y"}},
        {sim::scheme_c::IoState{"DO_VALVE", false, true}});

    sim::scheme_c::AxisState x_axis{"X"};
    x_axis.homed = true;
    x_axis.home_signal = true;
    x_axis.position_mm = 1.0;
    x_axis.velocity_mm_per_s = 0.25;
    x_axis.command_position_mm = 1.25;
    x_axis.command_velocity_mm_per_s = 0.0;
    x_axis.following_error_mm = 0.25;
    x_axis.encoder_quantization_mm = 0.25;
    x_axis.following_error_active = true;

    recorder->recordSnapshot(
        second_tick,
        {x_axis, sim::scheme_c::AxisState{"Y"}},
        {sim::scheme_c::IoState{"DO_VALVE", true, true}});

    auto result = recorder->finish(
        second_tick,
        {x_axis, sim::scheme_c::AxisState{"Y"}},
        {sim::scheme_c::IoState{"DO_VALVE", true, true}});
    sim::scheme_c::finalizeRecordingResult(result, sim::scheme_c::SessionStatus::Completed, "bridge_completed");

    require(result.motion_profile.size() == 4U, "recorder: expected one motion sample per axis per snapshot");
    require(result.trace.size() == 4U, "recorder: expected axis and io trace entries");
    require(result.timeline.size() == 5U, "recorder: expected merged event + trace timeline");
    require(result.timeline.front().kind == "event", "recorder: timeline should keep event before state changes");
    require(result.trace[0].subject == "X", "recorder: axis trace subject mismatch");
    require(result.trace[0].field == "homed", "recorder: axis trace field mismatch");
    require(result.trace[3].scope == "io", "recorder: expected io trace entry");
    require(result.summary.terminal_state == "completed", "recorder: summary terminal state mismatch");
    require(result.summary.motion_sample_count == 4U, "recorder: summary motion sample count mismatch");
    require(!result.summary.empty_timeline, "recorder: summary should mark non-empty timeline");
    require(result.summary.following_error_sample_count == 1U,
            "recorder: summary following error sample count mismatch");
    require(result.summary.max_following_error_mm == 0.25,
            "recorder: summary max following error mismatch");
    require(result.summary.mean_following_error_mm == 0.0625,
            "recorder: summary mean following error mismatch");
}

void testRecordingResultNormalizesStoppedSessionWithoutSnapshots() {
    sim::scheme_c::SimulationSessionConfig config;
    config.axis_names = {"X"};
    config.io_channels = {"DO_VALVE"};
    config.tick = std::chrono::milliseconds(1);

    auto session = sim::scheme_c::createBaselineSession(config);
    session.start();
    session.stop("manual stop before first tick");
    const auto result = session.finish();

    require(result.status == sim::scheme_c::SessionStatus::Stopped, "session: expected stopped status");
    require(result.recording.motion_profile.empty(), "session: stopped session should not emit motion profile");
    require(result.recording.summary.terminal_state == "stopped", "session: stopped summary state mismatch");
    require(result.recording.summary.termination_reason == "application_requested_stop",
            "session: stopped summary reason mismatch");
    require(!result.recording.timeline.empty(), "session: stopped session should emit stop timeline event");
}

void testRecordingResultNormalizesFailedSessionWithoutDependencies() {
    sim::scheme_c::SimulationSessionConfig config;
    sim::scheme_c::SimulationSession session(
        config,
        nullptr,
        sim::scheme_c::createInMemoryVirtualAxisGroup(),
        sim::scheme_c::createInMemoryVirtualIo(),
        sim::scheme_c::createTimelineRecorder());

    const auto result = session.run();

    require(result.status == sim::scheme_c::SessionStatus::Failed, "session: expected failed status");
    require(result.recording.timeline.empty(), "session: failed session should keep empty timeline");
    require(result.recording.summary.terminal_state == "failed", "session: failed summary state mismatch");
    require(result.recording.summary.termination_reason == "session_dependencies_missing",
            "session: failed summary reason mismatch");
}

void testSchemeCResultExporterSerializesCanonicalFields() {
    auto recorder = sim::scheme_c::createTimelineRecorder();
    const sim::scheme_c::TickInfo tick{0U, std::chrono::milliseconds(0), std::chrono::milliseconds(1)};

    sim::scheme_c::AxisState axis{"X"};
    axis.homed = true;
    axis.position_mm = 2.0;
    axis.command_position_mm = 2.5;
    axis.following_error_mm = 0.5;
    axis.encoder_quantization_mm = 0.25;
    axis.following_error_active = true;
    recorder->recordEvent(tick, "RuntimeBridge", "initialized");
    recorder->recordSnapshot(tick, {axis}, {sim::scheme_c::IoState{"DO_VALVE", false, true}});

    sim::scheme_c::SimulationSessionResult result;
    result.status = sim::scheme_c::SessionStatus::Completed;
    result.bridge_bindings = sim::scheme_c::makeDefaultRuntimeBridgeBindings({"X"}, {"DO_VALVE"});
    result.runtime_bridge = sim::scheme_c::createRuntimeBridge(result.bridge_bindings)->metadata();
    result.recording = recorder->finish(tick, {axis}, {sim::scheme_c::IoState{"DO_VALVE", false, true}});
    sim::scheme_c::finalizeRecordingResult(result.recording, result.status, "bridge_completed");

    const auto json_text = sim::ResultExporter::toJson(result, false);
    const auto json = nlohmann::json::parse(json_text);

    require(json.at("schema_version").get<std::string>() == "scheme_c.recording.v1",
            "exporter: schema version mismatch");
    require(json.at("session_status").get<std::string>() == "completed", "exporter: session status mismatch");
    require(json.at("runtime_bridge").at("process_runtime_core_linked").get<bool>(),
            "exporter: runtime bridge linkage flag mismatch");
    require(json.at("runtime_bridge").at("follow_up_seams").empty(),
            "exporter: follow_up_seams should be empty after scheme C closure");
    require(json.at("recording").at("motion_profile").size() == 1U, "exporter: motion_profile size mismatch");
    require(json.at("recording").at("trace").size() == 2U, "exporter: trace size mismatch");
    require(json.at("recording").at("motion_profile").at(0).at("command_position_mm").get<double>() == 2.5,
            "exporter: command_position_mm mismatch");
    require(json.at("recording").at("motion_profile").at(0).at("following_error_mm").get<double>() == 0.5,
            "exporter: following_error_mm mismatch");
    require(json.at("recording").at("motion_profile").at(0).at("following_error_active").get<bool>(),
            "exporter: following_error_active mismatch");
    require(json.at("recording").at("summary").at("timeline_count").get<std::size_t>() ==
                json.at("recording").at("timeline").size(),
            "exporter: summary timeline_count mismatch");
    require(json.at("recording").at("summary").at("event_count").get<std::size_t>() ==
                json.at("recording").at("events").size(),
            "exporter: summary event_count mismatch");
    require(json.at("recording").at("summary").at("terminal_state").get<std::string>() == "completed",
            "exporter: summary state mismatch");
    require(json.at("recording").at("summary").at("max_following_error_mm").get<double>() == 0.5,
            "exporter: summary max_following_error_mm mismatch");
    require(json.at("recording").at("summary").at("following_error_sample_count").get<std::size_t>() == 1U,
            "exporter: summary following_error_sample_count mismatch");

    const auto path = std::filesystem::temp_directory_path() / "simulation_engine_scheme_c_result_export.json";
    sim::ResultExporter::writeJsonFile(result, path.string(), true);
    require(std::filesystem::exists(path), "exporter: scheme c result file missing");

    std::ifstream input(path);
    nlohmann::json file_json;
    input >> file_json;
    require(file_json.at("recording").at("timeline").size() == 3U, "exporter: persisted timeline mismatch");
    input.close();
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    try {
        testRecorderBuildsTraceTimelineAndSummary();
        testRecordingResultNormalizesStoppedSessionWithoutSnapshots();
        testRecordingResultNormalizesFailedSessionWithoutDependencies();
        testSchemeCResultExporterSerializesCanonicalFields();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
