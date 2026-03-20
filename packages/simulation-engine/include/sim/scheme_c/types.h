#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sim::scheme_c {

using Duration = std::chrono::nanoseconds;
using TickIndex = std::uint64_t;

struct TickInfo {
    TickIndex tick_index{0};
    Duration now{Duration::zero()};
    Duration step{std::chrono::milliseconds(1)};
};

struct AxisCommand {
    std::string axis;
    double target_position_mm{0.0};
    double target_velocity_mm_per_s{0.0};
    bool home_requested{false};
    bool stop_requested{false};
};

struct MotionRealismConfig {
    bool enabled{false};
    double servo_lag_seconds{0.0};
    double encoder_quantization_mm{0.0};
    double following_error_threshold_mm{0.0};
};

struct AxisState {
    std::string axis;
    double position_mm{0.0};
    double velocity_mm_per_s{0.0};
    double command_position_mm{0.0};
    double command_velocity_mm_per_s{0.0};
    double following_error_mm{0.0};
    double encoder_quantization_mm{0.0};
    double configured_velocity_mm_per_s{0.0};
    double configured_acceleration_mm_per_s2{0.0};
    double soft_limit_negative_mm{0.0};
    double soft_limit_positive_mm{0.0};
    bool enabled{false};
    bool homed{false};
    bool home_signal{false};
    bool index_signal{false};
    bool positive_soft_limit{false};
    bool negative_soft_limit{false};
    bool positive_hard_limit{false};
    bool negative_hard_limit{false};
    bool positive_hard_limit_enabled{true};
    bool negative_hard_limit_enabled{true};
    bool servo_alarm{false};
    bool has_error{false};
    int error_code{0};
    bool running{false};
    bool done{true};
    bool following_error_active{false};
};

struct IoState {
    std::string channel;
    bool value{false};
    bool output{true};
};

struct RecordedSnapshot {
    TickInfo tick{};
    std::vector<AxisState> axes{};
    std::vector<IoState> io{};
};

struct RecordedEvent {
    TickInfo tick{};
    std::string source;
    std::string message;
};

enum class SessionStatus {
    Idle,
    Running,
    Paused,
    Completed,
    Stopped,
    TimedOut,
    Failed
};

struct MotionProfileSample {
    TickInfo tick{};
    std::string axis;
    double position_mm{0.0};
    double velocity_mm_per_s{0.0};
    double command_position_mm{0.0};
    double command_velocity_mm_per_s{0.0};
    double following_error_mm{0.0};
    double encoder_quantization_mm{0.0};
    bool running{false};
    bool done{true};
    bool homed{false};
    bool has_error{false};
    int error_code{0};
    bool following_error_active{false};
};

struct StateTraceEntry {
    TickInfo tick{};
    std::string scope;
    std::string subject;
    std::string field;
    std::string previous_value;
    std::string current_value;
};

struct TimelineEntry {
    TickInfo tick{};
    std::string kind;
    std::string source;
    std::string subject;
    std::string field;
    std::string previous_value;
    std::string current_value;
    std::string message;
};

struct RecordingSummary {
    std::string terminal_state{"idle"};
    std::string termination_reason{"not-run"};
    TickIndex ticks_executed{0};
    Duration simulated_time{Duration::zero()};
    std::size_t snapshot_count{0};
    std::size_t event_count{0};
    std::size_t trace_count{0};
    std::size_t timeline_count{0};
    std::size_t motion_sample_count{0};
    std::size_t axis_count{0};
    std::size_t io_count{0};
    std::size_t following_error_sample_count{0};
    bool empty_timeline{true};
    bool has_error{false};
    double max_following_error_mm{0.0};
    double mean_following_error_mm{0.0};
};

struct RecordingResult {
    std::vector<RecordedSnapshot> snapshots{};
    std::vector<RecordedEvent> events{};
    std::vector<MotionProfileSample> motion_profile{};
    std::vector<StateTraceEntry> trace{};
    std::vector<TimelineEntry> timeline{};
    RecordingSummary summary{};
    std::vector<AxisState> final_axes{};
    std::vector<IoState> final_io{};
    TickIndex ticks_executed{0};
    Duration simulated_time{Duration::zero()};
};

}  // namespace sim::scheme_c
