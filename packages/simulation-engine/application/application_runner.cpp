#include "sim/scheme_c/application_runner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "sim/scheme_c/recorder.h"

namespace sim::scheme_c {
namespace {

using OrderedJson = nlohmann::ordered_json;

constexpr double kPointEpsilon = 1e-6;
constexpr double kPi = 3.14159265358979323846;

struct Point {
    double x{0.0};
    double y{0.0};
};

struct ParsedCanonicalInput {
    CanonicalSimulationInput input{};
    std::vector<std::string> notices{};
};

Point parsePoint(const nlohmann::json& node, const std::string& field_name) {
    if (!node.is_object()) {
        throw std::invalid_argument(field_name + " must be an object with x/y.");
    }

    if (!node.contains("x") || !node.contains("y")) {
        throw std::invalid_argument(field_name + " must contain x and y.");
    }

    const auto x = node.at("x").get<double>();
    const auto y = node.at("y").get<double>();
    if (!std::isfinite(x) || !std::isfinite(y)) {
        throw std::invalid_argument(field_name + " coordinates must be finite.");
    }

    return Point{x, y};
}

bool nearlyEqual(double lhs, double rhs, double epsilon = kPointEpsilon) {
    return std::abs(lhs - rhs) <= epsilon;
}

bool nearlyEqual(const Point& lhs, const Point& rhs, double epsilon = kPointEpsilon) {
    return nearlyEqual(lhs.x, rhs.x, epsilon) && nearlyEqual(lhs.y, rhs.y, epsilon);
}

void appendUnique(std::vector<std::string>& values, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::vector<std::string> parseStringArray(
    const nlohmann::json& root,
    const char* field_name,
    const std::vector<std::string>& default_values = {}) {
    if (!root.contains(field_name)) {
        return default_values;
    }

    const auto& node = root.at(field_name);
    if (!node.is_array()) {
        throw std::invalid_argument(std::string(field_name) + " must be an array of strings.");
    }

    std::vector<std::string> values;
    for (const auto& entry : node) {
        if (!entry.is_string()) {
            throw std::invalid_argument(std::string(field_name) + " entries must be strings.");
        }
        appendUnique(values, entry.get<std::string>());
    }

    return values;
}

Duration secondsToDuration(double seconds, const std::string& field_name) {
    if (!std::isfinite(seconds) || seconds <= 0.0) {
        throw std::invalid_argument(field_name + " must be a positive finite number.");
    }

    const auto duration = std::chrono::duration_cast<Duration>(std::chrono::duration<double>(seconds));
    if (duration <= Duration::zero()) {
        throw std::invalid_argument(field_name + " resolves to a non-positive duration.");
    }

    return duration;
}

Duration nonNegativeSecondsToDuration(double seconds, const std::string& field_name) {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        throw std::invalid_argument(field_name + " must be a non-negative finite number.");
    }

    return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(seconds));
}

MotionRealismConfig parseMotionRealismConfig(const nlohmann::json& root) {
    MotionRealismConfig config;
    if (!root.contains("motion_realism")) {
        return config;
    }

    const auto& node = root.at("motion_realism");
    if (!node.is_object()) {
        throw std::invalid_argument("motion_realism must be an object when provided.");
    }

    config.enabled = node.value("enabled", true);
    config.servo_lag_seconds = node.value("servo_lag_seconds", 0.0);
    config.encoder_quantization_mm = node.value("encoder_quantization_mm", 0.0);
    config.following_error_threshold_mm = node.value("following_error_threshold_mm", 0.0);
    return config;
}

TickIndex deriveMaxTicks(const Duration timeout, const Duration tick) {
    const auto timeout_ns = static_cast<long double>(timeout.count());
    const auto tick_ns = static_cast<long double>(tick.count());
    if (timeout_ns <= 0.0L || tick_ns <= 0.0L) {
        return 1U;
    }

    const auto ticks = static_cast<long double>(std::ceil(timeout_ns / tick_ns));
    const auto safe_ticks = std::max<long double>(ticks + 1.0L, 1.0L);
    return static_cast<TickIndex>(safe_ticks);
}

Point pointFromPolar(const Point& center, double radius, double angle_radians) {
    return Point{
        center.x + radius * std::cos(angle_radians),
        center.y + radius * std::sin(angle_radians)
    };
}

RuntimePathSegmentType inferArcType(const nlohmann::json& segment_node, double start_angle, double end_angle) {
    if (segment_node.contains("clockwise")) {
        return segment_node.at("clockwise").get<bool>()
            ? RuntimePathSegmentType::ArcClockwise
            : RuntimePathSegmentType::ArcCounterClockwise;
    }

    if (segment_node.contains("direction")) {
        const auto direction = segment_node.at("direction").get<std::string>();
        if (direction == "CW") {
            return RuntimePathSegmentType::ArcClockwise;
        }
        if (direction == "CCW") {
            return RuntimePathSegmentType::ArcCounterClockwise;
        }
        throw std::invalid_argument("ARC direction must be CW or CCW when provided.");
    }

    const auto delta = end_angle - start_angle;
    if (nearlyEqual(delta, 0.0)) {
        throw std::invalid_argument("ARC start_angle and end_angle must not be equal.");
    }

    return delta < 0.0
        ? RuntimePathSegmentType::ArcClockwise
        : RuntimePathSegmentType::ArcCounterClockwise;
}

ParsedCanonicalInput parseCanonicalSimulationInput(const nlohmann::json& root, const std::string& source_name) {
    if (!root.is_object()) {
        throw std::invalid_argument("Canonical simulation input must be a JSON object.");
    }

    ParsedCanonicalInput parsed;
    parsed.input.axis_names = parseStringArray(root, "axis_names", {"X", "Y"});
    parsed.input.io_channels = parseStringArray(root, "io_channels", {});
    parsed.input.home_before_run = root.value("home_before_run", true);
    parsed.input.motion_realism = parseMotionRealismConfig(root);

    const auto timestep_seconds = root.value("timestep_seconds", 0.001);
    parsed.input.tick = secondsToDuration(timestep_seconds, "timestep_seconds");

    const auto max_time_seconds = root.value("max_simulation_time_seconds", 600.0);
    parsed.input.timeout = secondsToDuration(max_time_seconds, "max_simulation_time_seconds");
    parsed.input.max_ticks = deriveMaxTicks(*parsed.input.timeout, parsed.input.tick);

    parsed.input.path.max_velocity_mm_per_s = root.value("max_velocity", 220.0);
    parsed.input.path.max_acceleration_mm_per_s2 = root.value("max_acceleration", 600.0);
    parsed.input.path.sample_dt_s = timestep_seconds;

    if (!std::isfinite(parsed.input.path.max_velocity_mm_per_s) ||
        parsed.input.path.max_velocity_mm_per_s <= 0.0) {
        throw std::invalid_argument("max_velocity must be a positive finite number.");
    }
    if (!std::isfinite(parsed.input.path.max_acceleration_mm_per_s2) ||
        parsed.input.path.max_acceleration_mm_per_s2 <= 0.0) {
        throw std::invalid_argument("max_acceleration must be a positive finite number.");
    }

    if (!root.contains("segments") || !root.at("segments").is_array() || root.at("segments").empty()) {
        throw std::invalid_argument("segments must be a non-empty array.");
    }

    Point first_start{};
    Point cursor{};
    bool has_cursor = false;

    for (const auto& segment_node : root.at("segments")) {
        if (!segment_node.is_object()) {
            throw std::invalid_argument("Each segment must be an object.");
        }

        if (!segment_node.contains("type") || !segment_node.at("type").is_string()) {
            throw std::invalid_argument("Each segment must contain a string type.");
        }

        const auto type = segment_node.at("type").get<std::string>();
        if (type == "LINE") {
            const auto start = parsePoint(segment_node.at("start"), "LINE.start");
            const auto end = parsePoint(segment_node.at("end"), "LINE.end");

            if (!has_cursor) {
                first_start = start;
                cursor = start;
                has_cursor = true;
            }

            if (!nearlyEqual(cursor, start)) {
                throw std::invalid_argument("LINE.start must match the previous segment end.");
            }

            parsed.input.path.segments.push_back(RuntimeBridgePathSegment{
                RuntimePathSegmentType::Line,
                end.x,
                end.y,
                0.0,
                0.0
            });
            cursor = end;
            continue;
        }

        if (type == "ARC") {
            const auto center = parsePoint(segment_node.at("center"), "ARC.center");
            const auto radius = segment_node.at("radius").get<double>();
            const auto start_angle = segment_node.at("start_angle").get<double>();
            const auto end_angle = segment_node.at("end_angle").get<double>();

            if (!std::isfinite(radius) || radius <= 0.0) {
                throw std::invalid_argument("ARC.radius must be a positive finite number.");
            }
            if (!std::isfinite(start_angle) || !std::isfinite(end_angle)) {
                throw std::invalid_argument("ARC angles must be finite.");
            }

            const auto start = pointFromPolar(center, radius, start_angle);
            const auto end = pointFromPolar(center, radius, end_angle);

            if (!has_cursor) {
                first_start = start;
                cursor = start;
                has_cursor = true;
            }

            if (!nearlyEqual(cursor, start)) {
                throw std::invalid_argument("ARC start point must match the previous segment end.");
            }

            parsed.input.path.segments.push_back(RuntimeBridgePathSegment{
                inferArcType(segment_node, start_angle, end_angle),
                end.x,
                end.y,
                center.x,
                center.y
            });
            cursor = end;
            continue;
        }

        throw std::invalid_argument("Unsupported segment type: " + type);
    }

    if (!parsed.input.path.segments.empty() &&
        (!nearlyEqual(first_start.x, 0.0) || !nearlyEqual(first_start.y, 0.0))) {
        parsed.input.initial_move = RuntimeBridgeMoveCommand{
            first_start.x,
            first_start.y,
            parsed.input.path.max_velocity_mm_per_s
        };
    }

    if (root.contains("io_delays")) {
        const auto& io_delays = root.at("io_delays");
        if (!io_delays.is_array()) {
            throw std::invalid_argument("io_delays must be an array when provided.");
        }
        for (const auto& delay : io_delays) {
            if (!delay.is_object() || !delay.contains("channel") || !delay.at("channel").is_string()) {
                throw std::invalid_argument("io_delays entries must contain a string channel.");
            }
            if (!delay.contains("delay_seconds")) {
                throw std::invalid_argument("io_delays entries must contain delay_seconds.");
            }

            const auto channel = delay.at("channel").get<std::string>();
            appendUnique(parsed.input.io_channels, channel);
            parsed.input.replay_plan.io_delays.push_back(ReplayIoDelaySpec{
                channel,
                nonNegativeSecondsToDuration(delay.at("delay_seconds").get<double>(), "io_delays.delay_seconds")
            });
        }
    }

    if (root.contains("triggers")) {
        const auto& triggers = root.at("triggers");
        if (!triggers.is_array()) {
            throw std::invalid_argument("triggers must be an array when provided.");
        }
        for (const auto& trigger : triggers) {
            if (!trigger.is_object() || !trigger.contains("channel") || !trigger.at("channel").is_string()) {
                throw std::invalid_argument("triggers entries must contain a string channel.");
            }

            if (!trigger.contains("trigger_position")) {
                throw std::invalid_argument("triggers entries must contain trigger_position.");
            }
            if (!trigger.contains("state")) {
                throw std::invalid_argument("triggers entries must contain state.");
            }

            const auto channel = trigger.at("channel").get<std::string>();
            const auto position = trigger.at("trigger_position").get<double>();
            if (!std::isfinite(position) || position < 0.0) {
                throw std::invalid_argument("triggers.trigger_position must be a non-negative finite number.");
            }

            appendUnique(parsed.input.io_channels, channel);
            parsed.input.replay_plan.triggers.push_back(ReplayTriggerSpec{
                channel,
                position,
                trigger.at("state").get<bool>(),
                0,
                ReplayTriggerSpace::PathProgress,
                0U
            });
        }
    }

    if (root.contains("valve") && !root.at("valve").is_object()) {
        throw std::invalid_argument("valve must be an object when provided.");
    }

    if (root.contains("valve") && root.at("valve").is_object()) {
        const auto& valve = root.at("valve");
        ReplayValveSpec valve_spec;
        valve_spec.channel = "DO_VALVE";
        if (valve.contains("channel")) {
            if (!valve.at("channel").is_string()) {
                throw std::invalid_argument("valve.channel must be a string when provided.");
            }
            valve_spec.channel = valve.at("channel").get<std::string>();
        }
        if (!valve.contains("open_delay_seconds") || !valve.contains("close_delay_seconds")) {
            throw std::invalid_argument("valve requires open_delay_seconds and close_delay_seconds.");
        }

        valve_spec.open_delay = nonNegativeSecondsToDuration(
            valve.at("open_delay_seconds").get<double>(),
            "valve.open_delay_seconds");
        valve_spec.close_delay = nonNegativeSecondsToDuration(
            valve.at("close_delay_seconds").get<double>(),
            "valve.close_delay_seconds");
        parsed.input.replay_plan.valve = valve_spec;
        appendUnique(parsed.input.io_channels, valve_spec.channel);
    }

    if (root.contains("stop")) {
        const auto& stop_node = root.at("stop");
        if (!stop_node.is_object()) {
            throw std::invalid_argument("stop must be an object when provided.");
        }

        StopDirective stop;
        if (stop_node.contains("after_ticks")) {
            const auto after_ticks = stop_node.at("after_ticks").get<std::int64_t>();
            if (after_ticks < 0) {
                throw std::invalid_argument("stop.after_ticks must be non-negative.");
            }
            stop.after_tick = static_cast<TickIndex>(after_ticks);
        }
        if (stop_node.contains("after_seconds")) {
            stop.after_time = secondsToDuration(stop_node.at("after_seconds").get<double>(), "stop.after_seconds");
        }
        if (!stop.after_tick.has_value() && !stop.after_time.has_value()) {
            throw std::invalid_argument("stop requires after_ticks or after_seconds.");
        }
        if (stop.after_tick.has_value() && stop.after_time.has_value()) {
            throw std::invalid_argument("stop must use only one of after_ticks or after_seconds.");
        }
        stop.immediate = stop_node.value("immediate", true);
        stop.clear_pending_commands = stop_node.value("clear_pending_commands", true);
        parsed.input.stop = stop;
    }

    if (!source_name.empty()) {
        parsed.notices.push_back("Loaded canonical simulation input from " + source_name + ".");
    }

    return parsed;
}

std::vector<AxisState> axisStatesForNames(const std::vector<std::string>& axis_names) {
    std::vector<AxisState> axes;
    axes.reserve(axis_names.size());
    for (const auto& axis_name : axis_names) {
        axes.push_back(AxisState{axis_name});
    }
    return axes;
}

std::vector<IoState> ioStatesForNames(const std::vector<std::string>& io_channels) {
    std::vector<IoState> io_states;
    io_states.reserve(io_channels.size());
    for (const auto& channel : io_channels) {
        io_states.push_back(IoState{channel, false, true});
    }
    return io_states;
}

SimulationSessionResult makeStructuredResult(
    SessionStatus status,
    const std::string& termination_reason,
    const std::string& message,
    const std::vector<std::string>& axis_names,
    const std::vector<std::string>& io_channels,
    const RuntimeBridgeBindings& bindings) {
    auto recorder = createTimelineRecorder();
    const TickInfo tick{0U, Duration::zero(), std::chrono::milliseconds(1)};
    recorder->recordEvent(tick, "ApplicationRunner", message);

    SimulationSessionResult result;
    result.status = status;
    result.bridge_bindings = bindings;

    auto bridge = createRuntimeBridge(bindings);
    result.runtime_bridge = bridge->metadata();
    result.recording = recorder->finish(tick, axisStatesForNames(axis_names), ioStatesForNames(io_channels));
    finalizeRecordingResult(result.recording, status, termination_reason);
    return result;
}

RuntimeBridgeBindings resolvedBindingsForInput(const CanonicalSimulationInput& input) {
    if (!input.bridge_bindings.axis_bindings.empty() || !input.bridge_bindings.io_bindings.empty()) {
        return input.bridge_bindings;
    }

    return makeDefaultRuntimeBridgeBindings(input.axis_names, input.io_channels);
}

std::optional<std::string> validateInput(const CanonicalSimulationInput& input) {
    if (input.axis_names.empty()) {
        return "axis_names must not be empty.";
    }
    if (std::find(input.axis_names.begin(), input.axis_names.end(), "X") == input.axis_names.end() ||
        std::find(input.axis_names.begin(), input.axis_names.end(), "Y") == input.axis_names.end()) {
        return "axis_names must contain X and Y.";
    }
    if (input.tick <= Duration::zero()) {
        return "tick must be positive.";
    }
    if (input.timeout.has_value() && *input.timeout <= Duration::zero()) {
        return "timeout must be positive when provided.";
    }
    if (input.max_ticks == 0U) {
        return "max_ticks must be greater than zero.";
    }
    if (!std::isfinite(input.motion_realism.servo_lag_seconds) || input.motion_realism.servo_lag_seconds < 0.0) {
        return "motion_realism.servo_lag_seconds must be a non-negative finite number.";
    }
    if (!std::isfinite(input.motion_realism.encoder_quantization_mm) ||
        input.motion_realism.encoder_quantization_mm < 0.0) {
        return "motion_realism.encoder_quantization_mm must be a non-negative finite number.";
    }
    if (!std::isfinite(input.motion_realism.following_error_threshold_mm) ||
        input.motion_realism.following_error_threshold_mm < 0.0) {
        return "motion_realism.following_error_threshold_mm must be a non-negative finite number.";
    }
    if (input.motion_realism.encoder_quantization_mm > 0.0 &&
        input.motion_realism.following_error_threshold_mm > 0.0 &&
        input.motion_realism.following_error_threshold_mm + 1e-9 <
            input.motion_realism.encoder_quantization_mm * 0.5) {
        return "motion_realism.following_error_threshold_mm must be >= encoder_quantization_mm / 2 when provided.";
    }
    if (input.path.segments.empty()) {
        return "path.segments must not be empty.";
    }
    if (!std::isfinite(input.path.max_velocity_mm_per_s) || input.path.max_velocity_mm_per_s <= 0.0) {
        return "path.max_velocity_mm_per_s must be positive.";
    }
    if (!std::isfinite(input.path.max_acceleration_mm_per_s2) || input.path.max_acceleration_mm_per_s2 <= 0.0) {
        return "path.max_acceleration_mm_per_s2 must be positive.";
    }
    if (!std::isfinite(input.path.sample_dt_s) || input.path.sample_dt_s <= 0.0) {
        return "path.sample_dt_s must be positive.";
    }
    if (input.stop.has_value()) {
        const auto& stop = *input.stop;
        if (!stop.after_tick.has_value() && !stop.after_time.has_value()) {
            return "stop requires after_tick or after_time.";
        }
        if (stop.after_tick.has_value() && stop.after_time.has_value()) {
            return "stop must use only one of after_tick or after_time.";
        }
        if (stop.after_time.has_value() && *stop.after_time <= Duration::zero()) {
            return "stop.after_time must be positive.";
        }
    }

    for (const auto& delay : input.replay_plan.io_delays) {
        if (delay.channel.empty()) {
            return "replay_plan.io_delays.channel must not be empty.";
        }
        if (delay.delay < Duration::zero()) {
            return "replay_plan.io_delays.delay must be non-negative.";
        }
    }

    for (const auto& trigger : input.replay_plan.triggers) {
        if (trigger.channel.empty()) {
            return "replay_plan.triggers.channel must not be empty.";
        }
        if (!std::isfinite(trigger.position_mm) || trigger.position_mm < 0.0) {
            return "replay_plan.triggers.position_mm must be a non-negative finite number.";
        }
    }

    if (input.replay_plan.valve.has_value()) {
        if (input.replay_plan.valve->channel.empty()) {
            return "replay_plan.valve.channel must not be empty.";
        }
        if (input.replay_plan.valve->open_delay < Duration::zero() ||
            input.replay_plan.valve->close_delay < Duration::zero()) {
            return "replay_plan.valve delays must be non-negative.";
        }
    }

    return std::nullopt;
}

SimulationSessionResult runMinimalClosedLoopWithNotices(
    const CanonicalSimulationInput& input,
    const std::vector<std::string>& notices) {
    const auto validation_error = validateInput(input);
    const auto bindings = resolvedBindingsForInput(input);
    if (validation_error.has_value()) {
        return makeStructuredResult(
            SessionStatus::Failed,
            (validation_error->find("axis_names must contain X and Y") != std::string::npos)
                ? "missing_required_axes"
                : "invalid_input",
            *validation_error,
            input.axis_names,
            input.io_channels,
            bindings);
    }

    SimulationSessionConfig session_config;
    session_config.axis_names = input.axis_names;
    session_config.io_channels = input.io_channels;
    session_config.bridge_bindings = bindings;
    session_config.replay_plan = input.replay_plan;
    session_config.motion_realism = input.motion_realism;
    session_config.tick = input.tick;
    session_config.timeout = input.timeout;
    session_config.max_ticks = input.max_ticks;

    auto session = createBaselineSession(session_config);
    auto* control = runtimeBridgeControl(session.runtimeBridge());
    if (control == nullptr) {
        return makeStructuredResult(
            SessionStatus::Failed,
            "bridge_control_unavailable",
            "Runtime bridge control is unavailable for minimal closed loop execution.",
            input.axis_names,
            input.io_channels,
            bindings);
    }

    if (input.home_before_run) {
        control->submitHome(RuntimeBridgeHomeCommand{});
    }
    if (input.initial_move.has_value()) {
        control->submitMove(*input.initial_move);
    }
    control->submitPath(input.path);

    session.start();
    for (const auto& notice : notices) {
        session.recorder().recordEvent(session.clock().current(), "ApplicationRunner", notice);
    }

    if (input.stop.has_value()) {
        const auto stop_command = RuntimeBridgeStopCommand{
            input.stop->immediate,
            input.stop->clear_pending_commands
        };

        TickIndex stop_tick = 0U;
        if (input.stop->after_tick.has_value()) {
            stop_tick = *input.stop->after_tick;
        } else if (input.stop->after_time.has_value()) {
            const auto after_ns = static_cast<long double>(input.stop->after_time->count());
            const auto tick_ns = static_cast<long double>(input.tick.count());
            stop_tick = static_cast<TickIndex>(std::ceil(after_ns / tick_ns));
        }

        session.scheduler().scheduleOnceAt(
            stop_tick,
            TickTaskPhase::Default,
            [&session, control, stop_command](const TickInfo& tick) {
                const auto stop_id = control->submitStop(stop_command);
                session.recorder().recordEvent(
                    tick,
                    "ApplicationRunner",
                    "Submitted stop command #" + std::to_string(stop_id) + ".");
            },
            "application_runner_stop");
    }

    return session.run();
}

}  // namespace

CanonicalSimulationInput loadCanonicalSimulationInputJson(
    const std::string& json_text,
    const std::string& source_name) {
    const auto root = OrderedJson::parse(json_text);
    return parseCanonicalSimulationInput(root, source_name).input;
}

CanonicalSimulationInput loadCanonicalSimulationInputFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open canonical simulation input: " + path);
    }

    OrderedJson root;
    input >> root;
    return parseCanonicalSimulationInput(root, path).input;
}

SimulationSessionResult runMinimalClosedLoop(const CanonicalSimulationInput& input) {
    return runMinimalClosedLoopWithNotices(input, {});
}

SimulationSessionResult runMinimalClosedLoopFile(const std::string& input_path) {
    try {
        std::ifstream input_stream(input_path);
        if (!input_stream) {
            return makeStructuredResult(
                SessionStatus::Failed,
                "invalid_input",
                "Failed to open canonical simulation input: " + input_path,
                {"X", "Y"},
                {},
                {});
        }

        OrderedJson root;
        input_stream >> root;
        const auto parsed = parseCanonicalSimulationInput(root, input_path);
        return runMinimalClosedLoopWithNotices(parsed.input, parsed.notices);
    } catch (const std::exception& ex) {
        return makeStructuredResult(
            SessionStatus::Failed,
            "invalid_input",
            ex.what(),
            {"X", "Y"},
            {},
            {});
    }
}

}  // namespace sim::scheme_c
