#include "sim/io/result_exporter.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace {

using OrderedJson = nlohmann::ordered_json;

std::int64_t durationToNanoseconds(const sim::scheme_c::Duration duration) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

std::string sessionStatusToString(const sim::scheme_c::SessionStatus status) {
    switch (status) {
    case sim::scheme_c::SessionStatus::Idle:
        return "idle";
    case sim::scheme_c::SessionStatus::Running:
        return "running";
    case sim::scheme_c::SessionStatus::Paused:
        return "paused";
    case sim::scheme_c::SessionStatus::Completed:
        return "completed";
    case sim::scheme_c::SessionStatus::Stopped:
        return "stopped";
    case sim::scheme_c::SessionStatus::TimedOut:
        return "timed_out";
    case sim::scheme_c::SessionStatus::Failed:
        return "failed";
    }

    return "unknown";
}

OrderedJson eventToJson(const sim::TimelineEvent& event) {
    return OrderedJson{
        {"time", event.time},
        {"channel", event.channel},
        {"state", event.state},
        {"source", event.source}
    };
}

OrderedJson motionSampleToJson(const sim::MotionSample& sample) {
    return OrderedJson{
        {"time", sample.time},
        {"position", sample.position},
        {"velocity", sample.velocity},
        {"target_velocity", sample.target_velocity},
        {"segment_index", sample.segment_index},
        {"segment_progress", sample.segment_progress}
    };
}

OrderedJson tickToJson(const sim::scheme_c::TickInfo& tick) {
    return OrderedJson{
        {"tick_index", tick.tick_index},
        {"time_ns", durationToNanoseconds(tick.now)},
        {"step_ns", durationToNanoseconds(tick.step)}
    };
}

OrderedJson axisStateToJson(const sim::scheme_c::AxisState& state) {
    return OrderedJson{
        {"axis", state.axis},
        {"position_mm", state.position_mm},
        {"velocity_mm_per_s", state.velocity_mm_per_s},
        {"command_position_mm", state.command_position_mm},
        {"command_velocity_mm_per_s", state.command_velocity_mm_per_s},
        {"following_error_mm", state.following_error_mm},
        {"encoder_quantization_mm", state.encoder_quantization_mm},
        {"configured_velocity_mm_per_s", state.configured_velocity_mm_per_s},
        {"configured_acceleration_mm_per_s2", state.configured_acceleration_mm_per_s2},
        {"soft_limit_negative_mm", state.soft_limit_negative_mm},
        {"soft_limit_positive_mm", state.soft_limit_positive_mm},
        {"enabled", state.enabled},
        {"homed", state.homed},
        {"home_signal", state.home_signal},
        {"index_signal", state.index_signal},
        {"positive_soft_limit", state.positive_soft_limit},
        {"negative_soft_limit", state.negative_soft_limit},
        {"positive_hard_limit", state.positive_hard_limit},
        {"negative_hard_limit", state.negative_hard_limit},
        {"positive_hard_limit_enabled", state.positive_hard_limit_enabled},
        {"negative_hard_limit_enabled", state.negative_hard_limit_enabled},
        {"servo_alarm", state.servo_alarm},
        {"has_error", state.has_error},
        {"error_code", state.error_code},
        {"running", state.running},
        {"done", state.done},
        {"following_error_active", state.following_error_active}
    };
}

OrderedJson ioStateToJson(const sim::scheme_c::IoState& state) {
    return OrderedJson{
        {"channel", state.channel},
        {"value", state.value},
        {"output", state.output}
    };
}

OrderedJson snapshotToJson(const sim::scheme_c::RecordedSnapshot& snapshot) {
    OrderedJson json{
        {"tick", tickToJson(snapshot.tick)},
        {"axes", OrderedJson::array()},
        {"io", OrderedJson::array()}
    };

    for (const auto& axis : snapshot.axes) {
        json["axes"].push_back(axisStateToJson(axis));
    }

    for (const auto& io : snapshot.io) {
        json["io"].push_back(ioStateToJson(io));
    }

    return json;
}

OrderedJson recordedEventToJson(const sim::scheme_c::RecordedEvent& event) {
    return OrderedJson{
        {"tick", tickToJson(event.tick)},
        {"source", event.source},
        {"message", event.message}
    };
}

OrderedJson schemeCMotionSampleToJson(const sim::scheme_c::MotionProfileSample& sample) {
    return OrderedJson{
        {"tick", tickToJson(sample.tick)},
        {"axis", sample.axis},
        {"position_mm", sample.position_mm},
        {"velocity_mm_per_s", sample.velocity_mm_per_s},
        {"command_position_mm", sample.command_position_mm},
        {"command_velocity_mm_per_s", sample.command_velocity_mm_per_s},
        {"following_error_mm", sample.following_error_mm},
        {"encoder_quantization_mm", sample.encoder_quantization_mm},
        {"running", sample.running},
        {"done", sample.done},
        {"homed", sample.homed},
        {"has_error", sample.has_error},
        {"error_code", sample.error_code},
        {"following_error_active", sample.following_error_active}
    };
}

OrderedJson stateTraceToJson(const sim::scheme_c::StateTraceEntry& entry) {
    return OrderedJson{
        {"tick", tickToJson(entry.tick)},
        {"scope", entry.scope},
        {"subject", entry.subject},
        {"field", entry.field},
        {"previous_value", entry.previous_value},
        {"current_value", entry.current_value}
    };
}

OrderedJson timelineEntryToJson(const sim::scheme_c::TimelineEntry& entry) {
    return OrderedJson{
        {"tick", tickToJson(entry.tick)},
        {"kind", entry.kind},
        {"source", entry.source},
        {"subject", entry.subject},
        {"field", entry.field},
        {"previous_value", entry.previous_value},
        {"current_value", entry.current_value},
        {"message", entry.message}
    };
}

OrderedJson summaryToJson(const sim::scheme_c::RecordingSummary& summary) {
    return OrderedJson{
        {"terminal_state", summary.terminal_state},
        {"termination_reason", summary.termination_reason},
        {"ticks_executed", summary.ticks_executed},
        {"simulated_time_ns", durationToNanoseconds(summary.simulated_time)},
        {"snapshot_count", summary.snapshot_count},
        {"event_count", summary.event_count},
        {"trace_count", summary.trace_count},
        {"timeline_count", summary.timeline_count},
        {"motion_sample_count", summary.motion_sample_count},
        {"axis_count", summary.axis_count},
        {"io_count", summary.io_count},
        {"following_error_sample_count", summary.following_error_sample_count},
        {"empty_timeline", summary.empty_timeline},
        {"has_error", summary.has_error},
        {"max_following_error_mm", summary.max_following_error_mm},
        {"mean_following_error_mm", summary.mean_following_error_mm}
    };
}

OrderedJson runtimeBridgeBindingsToJson(const sim::scheme_c::RuntimeBridgeBindings& bindings) {
    OrderedJson json{
        {"axis_bindings", OrderedJson::array()},
        {"io_bindings", OrderedJson::array()}
    };

    for (const auto& axis_binding : bindings.axis_bindings) {
        json["axis_bindings"].push_back(OrderedJson{
            {"logical_axis_index", axis_binding.logical_axis_index},
            {"axis_name", axis_binding.axis_name}
        });
    }

    for (const auto& io_binding : bindings.io_bindings) {
        json["io_bindings"].push_back(OrderedJson{
            {"channel", io_binding.channel},
            {"signal_name", io_binding.signal_name},
            {"output", io_binding.output}
        });
    }

    return json;
}

OrderedJson runtimeBridgeMetadataToJson(const sim::scheme_c::RuntimeBridgeMetadata& metadata) {
    OrderedJson json{
        {"owner", metadata.owner},
        {"next_integration_point", metadata.next_integration_point},
        {"process_runtime_core_linked", metadata.process_runtime_core_linked},
        {"follow_up_seams", OrderedJson::array()}
    };

    switch (metadata.mode) {
    case sim::scheme_c::RuntimeBridgeMode::SeamStub:
        json["mode"] = "seam_stub";
        break;
    case sim::scheme_c::RuntimeBridgeMode::ProcessRuntimeCore:
        json["mode"] = "process_runtime_core";
        break;
    }

    for (const auto& seam : metadata.follow_up_seams) {
        json["follow_up_seams"].push_back(seam);
    }

    return json;
}

OrderedJson recordingToJson(const sim::scheme_c::RecordingResult& recording) {
    OrderedJson json{
        {"ticks_executed", recording.ticks_executed},
        {"simulated_time_ns", durationToNanoseconds(recording.simulated_time)},
        {"motion_profile", OrderedJson::array()},
        {"trace", OrderedJson::array()},
        {"timeline", OrderedJson::array()},
        {"summary", summaryToJson(recording.summary)},
        {"snapshots", OrderedJson::array()},
        {"events", OrderedJson::array()},
        {"final_axes", OrderedJson::array()},
        {"final_io", OrderedJson::array()}
    };

    for (const auto& sample : recording.motion_profile) {
        json["motion_profile"].push_back(schemeCMotionSampleToJson(sample));
    }

    for (const auto& entry : recording.trace) {
        json["trace"].push_back(stateTraceToJson(entry));
    }

    for (const auto& entry : recording.timeline) {
        json["timeline"].push_back(timelineEntryToJson(entry));
    }

    for (const auto& snapshot : recording.snapshots) {
        json["snapshots"].push_back(snapshotToJson(snapshot));
    }

    for (const auto& event : recording.events) {
        json["events"].push_back(recordedEventToJson(event));
    }

    for (const auto& axis : recording.final_axes) {
        json["final_axes"].push_back(axisStateToJson(axis));
    }

    for (const auto& io : recording.final_io) {
        json["final_io"].push_back(ioStateToJson(io));
    }

    return json;
}

void writeJsonFileContents(const std::string& json_text, const std::string& path) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open result file for writing: " + path);
    }

    output << json_text;
}

}  // namespace

namespace sim {

std::string ResultExporter::toJson(const SimulationResult& result, bool pretty) {
    OrderedJson root{
        {"total_time", result.total_time},
        {"motion_distance", result.motion_distance},
        {"valve_open_time", result.valve_open_time}
    };

    root["motion_profile"] = OrderedJson::array();
    for (const auto& sample : result.motion_profile) {
        root["motion_profile"].push_back(motionSampleToJson(sample));
    }

    root["io_events"] = OrderedJson::array();
    for (const auto& event : result.io_events) {
        root["io_events"].push_back(eventToJson(event));
    }

    root["valve_events"] = OrderedJson::array();
    for (const auto& event : result.valve_events) {
        root["valve_events"].push_back(eventToJson(event));
    }

    return pretty ? root.dump(2) : root.dump();
}

void ResultExporter::writeJsonFile(const SimulationResult& result,
                                   const std::string& path,
                                   bool pretty) {
    writeJsonFileContents(toJson(result, pretty), path);
}

std::string ResultExporter::toJson(const scheme_c::SimulationSessionResult& result, bool pretty) {
    OrderedJson root{
        {"schema_version", "scheme_c.recording.v1"},
        {"session_status", sessionStatusToString(result.status)},
        {"runtime_bridge", runtimeBridgeMetadataToJson(result.runtime_bridge)},
        {"bridge_bindings", runtimeBridgeBindingsToJson(result.bridge_bindings)},
        {"recording", recordingToJson(result.recording)}
    };

    return pretty ? root.dump(2) : root.dump();
}

void ResultExporter::writeJsonFile(const scheme_c::SimulationSessionResult& result,
                                   const std::string& path,
                                   bool pretty) {
    writeJsonFileContents(toJson(result, pretty), path);
}

}  // namespace sim
