#include "sim/scheme_c/recorder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace sim::scheme_c {
namespace {

std::string boolToString(bool value) {
    return value ? "true" : "false";
}

std::string intToString(int value) {
    return std::to_string(value);
}

std::string sessionStatusToString(SessionStatus status) {
    switch (status) {
    case SessionStatus::Idle:
        return "idle";
    case SessionStatus::Running:
        return "running";
    case SessionStatus::Paused:
        return "paused";
    case SessionStatus::Completed:
        return "completed";
    case SessionStatus::Stopped:
        return "stopped";
    case SessionStatus::TimedOut:
        return "timed_out";
    case SessionStatus::Failed:
        return "failed";
    }

    return "unknown";
}

void appendAxisStateChanges(const TickInfo& tick,
                            const AxisState& previous,
                            const AxisState& current,
                            std::vector<StateTraceEntry>& trace) {
    struct FieldSpec {
        const char* name;
        std::string (*to_string)(const AxisState&);
    };

    static const std::array<FieldSpec, 11> kFieldSpecs{{
        {"homed", [](const AxisState& state) { return boolToString(state.homed); }},
        {"home_signal", [](const AxisState& state) { return boolToString(state.home_signal); }},
        {"running", [](const AxisState& state) { return boolToString(state.running); }},
        {"done", [](const AxisState& state) { return boolToString(state.done); }},
        {"following_error_active", [](const AxisState& state) { return boolToString(state.following_error_active); }},
        {"positive_soft_limit", [](const AxisState& state) { return boolToString(state.positive_soft_limit); }},
        {"negative_soft_limit", [](const AxisState& state) { return boolToString(state.negative_soft_limit); }},
        {"positive_hard_limit", [](const AxisState& state) { return boolToString(state.positive_hard_limit); }},
        {"negative_hard_limit", [](const AxisState& state) { return boolToString(state.negative_hard_limit); }},
        {"has_error", [](const AxisState& state) { return boolToString(state.has_error); }},
        {"error_code", [](const AxisState& state) { return intToString(state.error_code); }},
    }};

    for (const auto& spec : kFieldSpecs) {
        const std::string previous_value = spec.to_string(previous);
        const std::string current_value = spec.to_string(current);
        if (previous_value == current_value) {
            continue;
        }

        trace.push_back(StateTraceEntry{
            tick,
            "axis",
            current.axis,
            spec.name,
            previous_value,
            current_value
        });
    }
}

void appendIoStateChanges(const TickInfo& tick,
                          const IoState& previous,
                          const IoState& current,
                          std::vector<StateTraceEntry>& trace) {
    const std::string previous_value = boolToString(previous.value);
    const std::string current_value = boolToString(current.value);
    if (previous_value == current_value) {
        return;
    }

    trace.push_back(StateTraceEntry{
        tick,
        "io",
        current.channel,
        "value",
        previous_value,
        current_value
    });
}

std::vector<MotionProfileSample> buildMotionProfile(const std::vector<RecordedSnapshot>& snapshots) {
    std::vector<MotionProfileSample> motion_profile;
    for (const auto& snapshot : snapshots) {
        for (const auto& axis : snapshot.axes) {
            motion_profile.push_back(MotionProfileSample{
                snapshot.tick,
                axis.axis,
                axis.position_mm,
                axis.velocity_mm_per_s,
                axis.command_position_mm,
                axis.command_velocity_mm_per_s,
                axis.following_error_mm,
                axis.encoder_quantization_mm,
                axis.running,
                axis.done,
                axis.homed,
                axis.has_error,
                axis.error_code,
                axis.following_error_active
            });
        }
    }

    return motion_profile;
}

std::vector<StateTraceEntry> buildTrace(const std::vector<RecordedSnapshot>& snapshots) {
    std::vector<StateTraceEntry> trace;
    std::vector<AxisState> previous_axes;
    std::vector<IoState> previous_io;

    for (const auto& snapshot : snapshots) {
        for (const auto& axis : snapshot.axes) {
            auto previous_it = std::find_if(
                previous_axes.begin(),
                previous_axes.end(),
                [&axis](const AxisState& candidate) { return candidate.axis == axis.axis; });
            const AxisState previous = previous_it != previous_axes.end() ? *previous_it : AxisState{axis.axis};
            appendAxisStateChanges(snapshot.tick, previous, axis, trace);
        }

        for (const auto& io : snapshot.io) {
            auto previous_it = std::find_if(
                previous_io.begin(),
                previous_io.end(),
                [&io](const IoState& candidate) { return candidate.channel == io.channel; });
            const IoState previous = previous_it != previous_io.end() ? *previous_it : IoState{io.channel, false, io.output};
            appendIoStateChanges(snapshot.tick, previous, io, trace);
        }

        previous_axes = snapshot.axes;
        previous_io = snapshot.io;
    }

    return trace;
}

std::vector<TimelineEntry> buildTimeline(const std::vector<RecordedEvent>& events,
                                         const std::vector<StateTraceEntry>& trace) {
    struct IndexedTimelineEntry {
        TimelineEntry entry{};
        std::size_t sequence{0};
        int priority{0};
    };

    std::vector<IndexedTimelineEntry> indexed_entries;
    indexed_entries.reserve(events.size() + trace.size());

    std::size_t sequence = 0;
    for (const auto& event : events) {
        indexed_entries.push_back(IndexedTimelineEntry{
            TimelineEntry{
                event.tick,
                "event",
                event.source,
                "",
                "",
                "",
                "",
                event.message
            },
            sequence++,
            0
        });
    }

    for (const auto& change : trace) {
        indexed_entries.push_back(IndexedTimelineEntry{
            TimelineEntry{
                change.tick,
                "state_change",
                change.scope,
                change.subject,
                change.field,
                change.previous_value,
                change.current_value,
                ""
            },
            sequence++,
            1
        });
    }

    std::stable_sort(
        indexed_entries.begin(),
        indexed_entries.end(),
        [](const IndexedTimelineEntry& lhs, const IndexedTimelineEntry& rhs) {
            if (lhs.entry.tick.tick_index != rhs.entry.tick.tick_index) {
                return lhs.entry.tick.tick_index < rhs.entry.tick.tick_index;
            }
            if (lhs.priority != rhs.priority) {
                return lhs.priority < rhs.priority;
            }
            return lhs.sequence < rhs.sequence;
        });

    std::vector<TimelineEntry> timeline;
    timeline.reserve(indexed_entries.size());
    for (const auto& indexed_entry : indexed_entries) {
        timeline.push_back(indexed_entry.entry);
    }

    return timeline;
}

void refreshSummary(RecordingResult& result) {
    result.summary.ticks_executed = result.ticks_executed;
    result.summary.simulated_time = result.simulated_time;
    result.summary.snapshot_count = result.snapshots.size();
    result.summary.event_count = result.events.size();
    result.summary.trace_count = result.trace.size();
    result.summary.timeline_count = result.timeline.size();
    result.summary.motion_sample_count = result.motion_profile.size();
    result.summary.axis_count = result.final_axes.size();
    result.summary.io_count = result.final_io.size();
    result.summary.empty_timeline = result.timeline.empty();
    result.summary.has_error = std::any_of(
        result.final_axes.begin(),
        result.final_axes.end(),
        [](const AxisState& axis) { return axis.has_error || axis.error_code != 0; });
    result.summary.following_error_sample_count = 0U;
    result.summary.max_following_error_mm = 0.0;
    result.summary.mean_following_error_mm = 0.0;

    double total_following_error_mm = 0.0;
    for (const auto& sample : result.motion_profile) {
        const double absolute_error_mm = std::abs(sample.following_error_mm);
        total_following_error_mm += absolute_error_mm;
        result.summary.max_following_error_mm =
            std::max(result.summary.max_following_error_mm, absolute_error_mm);
        if (sample.following_error_active) {
            ++result.summary.following_error_sample_count;
        }
    }

    if (!result.motion_profile.empty()) {
        result.summary.mean_following_error_mm =
            total_following_error_mm / static_cast<double>(result.motion_profile.size());
    }
}

class TimelineRecorder final : public Recorder {
public:
    void recordSnapshot(const TickInfo& tick,
                        const std::vector<AxisState>& axes,
                        const std::vector<IoState>& io) override {
        snapshots_.push_back(RecordedSnapshot{tick, axes, io});
    }

    void recordEvent(const TickInfo& tick,
                     const std::string& source,
                     const std::string& message) override {
        events_.push_back(RecordedEvent{tick, source, message});
    }

    RecordingResult finish(const TickInfo& final_tick,
                           const std::vector<AxisState>& final_axes,
                           const std::vector<IoState>& final_io) override {
        RecordingResult result;
        result.snapshots = snapshots_;
        result.events = events_;
        result.motion_profile = buildMotionProfile(result.snapshots);
        result.trace = buildTrace(result.snapshots);
        result.timeline = buildTimeline(result.events, result.trace);
        result.final_axes = final_axes;
        result.final_io = final_io;
        result.ticks_executed = final_tick.tick_index;
        result.simulated_time = final_tick.now;
        refreshSummary(result);
        return result;
    }

private:
    std::vector<RecordedSnapshot> snapshots_{};
    std::vector<RecordedEvent> events_{};
};

}  // namespace

std::unique_ptr<Recorder> createTimelineRecorder() {
    return std::make_unique<TimelineRecorder>();
}

void finalizeRecordingResult(RecordingResult& result,
                             SessionStatus status,
                             const std::string& termination_reason) {
    result.summary.terminal_state = sessionStatusToString(status);
    result.summary.termination_reason = termination_reason;
    refreshSummary(result);
}

RecordingResult makeEmptyRecordingResult(SessionStatus status,
                                         const std::string& termination_reason) {
    RecordingResult result;
    finalizeRecordingResult(result, status, termination_reason);
    return result;
}

}  // namespace sim::scheme_c
