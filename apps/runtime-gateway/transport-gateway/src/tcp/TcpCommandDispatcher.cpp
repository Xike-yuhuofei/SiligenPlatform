#include "TcpCommandDispatcher.h"
#include "JsonProtocol.h"
#include "MockIoControlService.h"
#include "shared/encoding/EncodingCodec.h"
#include "shared/types/AxisTypes.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Types.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/TraceContext.h"

#include "facades/tcp/TcpDispensingFacade.h"
#include "facades/tcp/TcpMotionFacade.h"
#include "facades/tcp/TcpRecipeFacade.h"
#include "facades/tcp/TcpSystemFacade.h"
#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "process_planning/contracts/configuration/ReadyZeroSpeedResolver.h"
#include "motion_planning/contracts/InterpolationTypes.h"
#include "runtime_execution/contracts/system/IRuntimeStatusExportPort.h"

#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <vector>
#include <thread>

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "TcpCommandDispatcher"

namespace {

namespace Application = Siligen::Application;
namespace TcpFacades = Siligen::Application::Facades::Tcp;

using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::float32;
using Siligen::Domain::Recipes::Serialization::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;
using Siligen::Domain::Recipes::ValueObjects::ImportConflict;
using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
using GatewayJsonProtocol = Siligen::Adapters::Tcp::JsonProtocol;

double ReadJsonDouble(const nlohmann::json& params, const char* key, double fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        return params.at(key).get<double>();
    } catch (...) {
        try {
            if (params.at(key).is_string()) {
                return std::stod(params.at(key).get<std::string>());
            }
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

size_t ReadJsonSizeT(const nlohmann::json& params, const char* key, size_t fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        const auto value = params.at(key);
        if (value.is_number_unsigned()) {
            return value.get<size_t>();
        }
        if (value.is_number_integer()) {
            const auto parsed = value.get<int64_t>();
            return parsed < 0 ? fallback : static_cast<size_t>(parsed);
        }
        if (value.is_string()) {
            const auto text = value.get<std::string>();
            const auto parsed = std::stoll(text);
            return parsed < 0 ? fallback : static_cast<size_t>(parsed);
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

constexpr float kMovePrecheckEpsilon = 1e-4f;
constexpr double kPreviewJsonCoordinatePrecisionMm = 1e-3;

std::optional<std::string> CheckMoveAxisLimitBeforeDispatch(
    const std::shared_ptr<TcpFacades::TcpMotionFacade>& motion_facade,
    LogicalAxisId axis_id,
    float current_position,
    float target_position) {
    const float delta = target_position - current_position;
    if (std::fabs(delta) <= kMovePrecheckEpsilon) {
        return std::nullopt;
    }

    const bool positive = delta > 0.0f;
    auto limit_result = motion_facade->ReadLimitStatus(axis_id, positive);
    if (limit_result.IsError()) {
        return limit_result.GetError().GetMessage();
    }
    if (!limit_result.Value()) {
        return std::nullopt;
    }

    if (positive) {
        return std::string("Cannot move in positive direction: Positive limit is triggered");
    }
    return std::string("Cannot move in negative direction: Negative limit is triggered");
}

void LogAxisSnapshot(
    const char* tag,
    const std::shared_ptr<TcpFacades::TcpMotionFacade>& motion_facade,
    Siligen::Shared::Types::LogicalAxisId axis_id) {
    if (!motion_facade) {
        return;
    }
    const char* axis_name = Siligen::Shared::Types::AxisName(axis_id);
    auto status_result = motion_facade->GetAxisMotionStatus(axis_id);
    if (status_result.IsSuccess()) {
        const auto& status = status_result.Value();
        SILIGEN_LOG_INFO_FMT_HELPER(
            "%s axis=%s state=%d pos=(%.3f,%.3f) vel=%.3f enabled=%d in_pos=%d alarm=%d soft+=%d soft-=%d hard+=%d hard-=%d follow=%d home=%d index=%d",
            tag,
            axis_name,
            static_cast<int>(status.state),
            status.position.x,
            status.position.y,
            status.velocity,
            status.enabled,
            status.in_position,
            status.servo_alarm,
            status.soft_limit_positive,
            status.soft_limit_negative,
            status.hard_limit_positive,
            status.hard_limit_negative,
            status.following_error,
            status.home_signal,
            status.index_signal);
    } else {
        SILIGEN_LOG_WARNING_FMT_HELPER("%s axis=%s status read failed: %s",
                                       tag,
                                       axis_name,
                                       status_result.GetError().GetMessage().c_str());
    }

}

Application::Services::Motion::Execution::MotionReadinessQuery BuildReadinessQuery(
    const std::string& active_job_state) {
    Application::Services::Motion::Execution::MotionReadinessQuery query;
    query.coord_sys = 1;
    query.active_job_state = active_job_state;
    query.active_job_transition_state =
        Application::Services::Motion::Execution::ParseExecutionTransitionState(active_job_state);
    return query;
}

std::string ResolveReadinessReasonCode(
    const Application::Services::Motion::Execution::MotionReadinessResult& result) {
    if (!result.reason_code.empty()) {
        return result.reason_code;
    }
    return Application::Services::Motion::Execution::ToString(result.reason);
}

std::string ResolveReadinessMessage(
    const Application::Services::Motion::Execution::MotionReadinessResult& result) {
    if (!result.message.empty()) {
        return result.message;
    }
    return ResolveReadinessReasonCode(result);
}

void FlushLogs() {
    auto logger = Siligen::Shared::Interfaces::ILoggingService::GetInstance();
    if (logger) {
        logger->Flush();
    }
}

struct DxfBounds {
    double x_min = 0.0;
    double x_max = 0.0;
    double y_min = 0.0;
    double y_max = 0.0;
    bool has_value = false;
};

std::string ToUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

LogicalAxisId AxisFromName(std::string axis_name) {
    axis_name = ToUpperAscii(std::move(axis_name));
    if (axis_name == "X") {
        return LogicalAxisId::X;
    }
    if (axis_name == "Y") {
        return LogicalAxisId::Y;
    }
    if (axis_name == "Z") {
        return LogicalAxisId::Z;
    }
    if (axis_name == "U") {
        return LogicalAxisId::U;
    }
    return LogicalAxisId::INVALID;
}

std::string ExtractFilename(const std::string& path) {
    try {
        std::filesystem::path fs_path(path);
        return fs_path.filename().string();
    } catch (...) {
        return path;
    }
}

bool ReadFileToBuffer(const std::string& path, std::vector<uint8_t>& buffer, std::string& error_message) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error_message = "File not found: " + path;
        return false;
    }

    std::ifstream::pos_type size = file.tellg();
    if (size <= 0) {
        error_message = "File is empty: " + path;
        return false;
    }

    buffer.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    if (!file) {
        error_message = "Failed to read file: " + path;
        return false;
    }

    return true;
}

DxfBounds ComputeBounds(const std::vector<Siligen::TrajectoryPoint>& points) {
    DxfBounds bounds;
    if (points.empty()) {
        return bounds;
    }

    bounds.x_min = points.front().position.x;
    bounds.x_max = points.front().position.x;
    bounds.y_min = points.front().position.y;
    bounds.y_max = points.front().position.y;
    bounds.has_value = true;

    for (const auto& point : points) {
        bounds.x_min = std::min<double>(bounds.x_min, point.position.x);
        bounds.x_max = std::max<double>(bounds.x_max, point.position.x);
        bounds.y_min = std::min<double>(bounds.y_min, point.position.y);
        bounds.y_max = std::max<double>(bounds.y_max, point.position.y);
    }
    return bounds;
}

constexpr size_t kPreviewPolylineMaxPoints = 4000;
constexpr size_t kPreviewGlueMaxPoints = 5000;

nlohmann::json BuildPreviewPolyline(const std::vector<Siligen::TrajectoryPoint>& points, size_t max_points) {
    nlohmann::json polyline = nlohmann::json::array();
    if (points.empty()) {
        return polyline;
    }

    const size_t safe_max_points = std::max<size_t>(2, max_points);
    size_t step = 1;
    if (points.size() > safe_max_points) {
        const double numerator = static_cast<double>(points.size() - 1);
        const double denominator = static_cast<double>(safe_max_points - 1);
        step = static_cast<size_t>(std::ceil(numerator / denominator));
        step = std::max<size_t>(1, step);
    }

    for (size_t i = 0; i < points.size(); i += step) {
        const auto& point = points[i];
        polyline.push_back({
            {"x", static_cast<double>(point.position.x)},
            {"y", static_cast<double>(point.position.y)}
        });
    }

    const auto& last_point = points.back();
    const bool last_missing = polyline.empty() ||
        std::abs(polyline.back().value("x", 0.0) - static_cast<double>(last_point.position.x)) > 1e-9 ||
        std::abs(polyline.back().value("y", 0.0) - static_cast<double>(last_point.position.y)) > 1e-9;
    if (last_missing) {
        polyline.push_back({
            {"x", static_cast<double>(last_point.position.x)},
            {"y", static_cast<double>(last_point.position.y)}
        });
    }
    return polyline;
}

double QuantizePreviewCoordinate(double value) {
    return std::round(value / kPreviewJsonCoordinatePrecisionMm) * kPreviewJsonCoordinatePrecisionMm;
}

nlohmann::json BuildPreviewPointJson(float32 x, float32 y) {
    return {
        {"x", QuantizePreviewCoordinate(static_cast<double>(x))},
        {"y", QuantizePreviewCoordinate(static_cast<double>(y))}
    };
}

bool ReadJsonBool(const nlohmann::json& params, const char* key, bool fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        const auto& value = params.at(key);
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }
        if (value.is_string()) {
            auto text = value.get<std::string>();
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (text == "1" || text == "true" || text == "yes" || text == "on") {
                return true;
            }
            if (text == "0" || text == "false" || text == "no" || text == "off") {
                return false;
            }
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::optional<bool> ReadOptionalJsonBool(const nlohmann::json& params, const char* key) {
    if (!params.contains(key)) {
        return std::nullopt;
    }
    const auto& value = params.at(key);
    try {
        if (value.is_boolean()) {
            return value.get<bool>();
        }
        if (value.is_number_integer()) {
            return value.get<int>() != 0;
        }
        if (value.is_string()) {
            auto text = value.get<std::string>();
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (text == "1" || text == "true" || text == "yes" || text == "on") {
                return true;
            }
            if (text == "0" || text == "false" || text == "no" || text == "off") {
                return false;
            }
        }
    } catch (...) {
        return std::nullopt;
    }
    return std::nullopt;
}

int ReadJsonInt(const nlohmann::json& params, const char* key, int fallback) {
    if (!params.contains(key)) {
        return fallback;
    }
    try {
        const auto& value = params.at(key);
        if (value.is_number_integer()) {
            return value.get<int>();
        }
        if (value.is_number_float()) {
            return static_cast<int>(value.get<double>());
        }
        if (value.is_string()) {
            return std::stoi(value.get<std::string>());
        }
    } catch (...) {
        return fallback;
    }
    return fallback;
}

std::string ToIso8601UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &t);
#else
    gmtime_r(&t, &utc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string HexEncodeUint64(uint64_t value) {
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << value;
    return oss.str();
}

nlohmann::json BuildPreviewSignaturePayload(const std::string& filepath, const nlohmann::json& params) {
    nlohmann::json payload = nlohmann::json::object();
    payload["filepath"] = filepath;
    payload["dry_run"] = ReadJsonBool(params, "dry_run", false);
    payload["dispensing_speed_mm_s"] = ReadJsonDouble(params, "dispensing_speed_mm_s", ReadJsonDouble(params, "speed_mm_s", 0.0));
    payload["dry_run_speed_mm_s"] = ReadJsonDouble(params, "dry_run_speed_mm_s", 0.0);
    payload["rapid_speed_mm_s"] = ReadJsonDouble(params, "rapid_speed_mm_s", 0.0);
    payload["optimize_path"] = ReadJsonBool(params, "optimize_path", true);
    payload["start_x"] = ReadJsonDouble(params, "start_x", 0.0);
    payload["start_y"] = ReadJsonDouble(params, "start_y", 0.0);
    payload["approximate_splines"] = ReadJsonBool(params, "approximate_splines", false);
    payload["two_opt_iterations"] = ReadJsonInt(params, "two_opt_iterations", 0);
    payload["spline_max_step_mm"] = ReadJsonDouble(params, "spline_max_step_mm", 0.0);
    payload["spline_max_error_mm"] = ReadJsonDouble(params, "spline_max_error_mm", 0.0);
    payload["arc_tolerance_mm"] = ReadJsonDouble(params, "arc_tolerance_mm", ReadJsonDouble(params, "arc_tolerance", 0.0));
    payload["continuity_tolerance_mm"] =
        ReadJsonDouble(params, "continuity_tolerance_mm", ReadJsonDouble(params, "continuity_tolerance", 0.0));
    payload["curve_chain_angle_deg"] = ReadJsonDouble(params, "curve_chain_angle_deg", 0.0);
    payload["curve_chain_max_segment_mm"] = ReadJsonDouble(params, "curve_chain_max_segment_mm", 0.0);
    payload["max_jerk"] = ReadJsonDouble(params, "max_jerk", 0.0);
    payload["use_hardware_trigger"] = ReadJsonBool(params, "use_hardware_trigger", true);
    payload["use_interpolation_planner"] = ReadJsonBool(params, "use_interpolation_planner", true);
    payload["interpolation_algorithm"] = ReadJsonInt(params, "interpolation_algorithm", 0);
    return payload;
}

std::string ComputePreviewRequestSignature(const std::string& filepath, const nlohmann::json& params) {
    return BuildPreviewSignaturePayload(filepath, params).dump();
}

Application::UseCases::Dispensing::PlanningRequest BuildPreviewPlanningRequest(
    const std::string& filepath,
    const nlohmann::json& params) {
    Application::UseCases::Dispensing::PlanningRequest request;
    request.dxf_filepath = filepath;
    request.trajectory_config = Siligen::Shared::Types::TrajectoryConfig();

    const double speed = ReadJsonDouble(params, "dispensing_speed_mm_s", ReadJsonDouble(params, "speed_mm_s", 0.0));
    if (speed > 0.0) {
        request.trajectory_config.max_velocity = static_cast<float32>(speed);
    }
    const double acceleration = ReadJsonDouble(params, "acceleration_mm_s2", 0.0);
    if (acceleration > 0.0) {
        request.trajectory_config.max_acceleration = static_cast<float32>(acceleration);
    }
    const double max_jerk = ReadJsonDouble(params, "max_jerk", 0.0);
    if (max_jerk > 0.0) {
        request.trajectory_config.max_jerk = static_cast<float32>(max_jerk);
    }

    request.trajectory_config.arc_tolerance = static_cast<float32>(
        ReadJsonDouble(params, "arc_tolerance_mm", ReadJsonDouble(params, "arc_tolerance", request.trajectory_config.arc_tolerance)));

    request.optimize_path = ReadJsonBool(params, "optimize_path", true);
    request.start_x = static_cast<float32>(ReadJsonDouble(params, "start_x", 0.0));
    request.start_y = static_cast<float32>(ReadJsonDouble(params, "start_y", 0.0));
    request.approximate_splines = ReadJsonBool(params, "approximate_splines", false);
    request.two_opt_iterations = ReadJsonInt(params, "two_opt_iterations", 0);
    request.spline_max_step_mm = static_cast<float32>(ReadJsonDouble(params, "spline_max_step_mm", 0.0));
    request.spline_max_error_mm = static_cast<float32>(ReadJsonDouble(params, "spline_max_error_mm", 0.0));
    request.continuity_tolerance_mm = static_cast<float32>(
        ReadJsonDouble(params, "continuity_tolerance_mm", ReadJsonDouble(params, "continuity_tolerance", 0.0)));
    request.curve_chain_angle_deg = static_cast<float32>(ReadJsonDouble(params, "curve_chain_angle_deg", 0.0));
    request.curve_chain_max_segment_mm = static_cast<float32>(ReadJsonDouble(params, "curve_chain_max_segment_mm", 0.0));
    request.use_hardware_trigger = ReadJsonBool(params, "use_hardware_trigger", true);
    request.use_interpolation_planner = ReadJsonBool(params, "use_interpolation_planner", true);
    const int algorithm_raw = ReadJsonInt(params, "interpolation_algorithm", 0);
    using InterpolationAlgorithm = Siligen::MotionPlanning::Contracts::InterpolationAlgorithm;
    if (algorithm_raw >= static_cast<int>(InterpolationAlgorithm::LINEAR) &&
        algorithm_raw <= static_cast<int>(InterpolationAlgorithm::CIRCULAR_ARRAY)) {
        request.interpolation_algorithm = static_cast<InterpolationAlgorithm>(algorithm_raw);
    }
    return request;
}

Application::UseCases::Dispensing::PreparePlanRuntimeOverrides BuildPreparePlanRuntimeOverrides(
    const std::string& filepath,
    const nlohmann::json& params) {
    Application::UseCases::Dispensing::PreparePlanRuntimeOverrides request;
    request.source_path = filepath;
    request.arc_tolerance_mm = static_cast<float32>(
        ReadJsonDouble(params, "arc_tolerance_mm", ReadJsonDouble(params, "arc_tolerance", 0.01)));
    request.use_hardware_trigger = ReadJsonBool(params, "use_hardware_trigger", true);
    request.dry_run = ReadJsonBool(params, "dry_run", false);

    const double dispensingSpeed = ReadJsonDouble(
        params,
        "dispensing_speed_mm_s",
        ReadJsonDouble(params, "speed_mm_s", 0.0));
    const double dryRunSpeed = ReadJsonDouble(params, "dry_run_speed_mm_s", 0.0);
    const double rapidSpeed = ReadJsonDouble(params, "rapid_speed_mm_s", 0.0);
    if (dispensingSpeed > 0.0) {
        request.dispensing_speed_mm_s = static_cast<float32>(dispensingSpeed);
    }
    if (dryRunSpeed > 0.0) {
        request.dry_run_speed_mm_s = static_cast<float32>(dryRunSpeed);
    }
    if (rapidSpeed > 0.0) {
        request.rapid_speed_mm_s = static_cast<float32>(rapidSpeed);
    }
    const double acceleration = ReadJsonDouble(params, "acceleration_mm_s2", 0.0);
    if (acceleration > 0.0) {
        request.acceleration_mm_s2 = static_cast<float32>(acceleration);
    }

    request.velocity_trace_enabled = ReadJsonBool(params, "velocity_trace_enabled", false);
    request.velocity_trace_interval_ms =
        static_cast<int32>(ReadJsonSizeT(params, "velocity_trace_interval_ms", 0));
    if (params.contains("velocity_trace_path")) {
        try {
            request.velocity_trace_path = params.at("velocity_trace_path").get<std::string>();
        } catch (...) {
            request.velocity_trace_path.clear();
        }
    }
    request.velocity_guard_enabled = ReadJsonBool(params, "velocity_guard_enabled", request.velocity_guard_enabled);
    request.velocity_guard_ratio =
        static_cast<float32>(ReadJsonDouble(params, "velocity_guard_ratio", request.velocity_guard_ratio));
    request.velocity_guard_abs_mm_s =
        static_cast<float32>(ReadJsonDouble(params, "velocity_guard_abs_mm_s", request.velocity_guard_abs_mm_s));
    request.velocity_guard_min_expected_mm_s = static_cast<float32>(
        ReadJsonDouble(params, "velocity_guard_min_expected_mm_s", request.velocity_guard_min_expected_mm_s));
    request.velocity_guard_grace_ms = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_grace_ms", request.velocity_guard_grace_ms));
    request.velocity_guard_interval_ms = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_interval_ms", request.velocity_guard_interval_ms));
    request.velocity_guard_max_consecutive = static_cast<int32>(
        ReadJsonDouble(params, "velocity_guard_max_consecutive", request.velocity_guard_max_consecutive));
    request.velocity_guard_stop_on_violation =
        ReadJsonBool(params, "velocity_guard_stop_on_violation", request.velocity_guard_stop_on_violation);
    return request;
}

Application::UseCases::Dispensing::PreparePlanRequest BuildPreparePlanRequest(
    const std::string& artifact_id,
    const std::string& filepath,
    const nlohmann::json& params) {
    Application::UseCases::Dispensing::PreparePlanRequest request;
    request.artifact_id = artifact_id;
    request.planning_request = BuildPreviewPlanningRequest(filepath, params);
    request.runtime_overrides = BuildPreparePlanRuntimeOverrides(filepath, params);
    return request;
}

double ResolveEffectivePreviewSpeed(
    const Application::UseCases::Dispensing::PreparePlanRuntimeOverrides& request) {
    return request.dry_run
        ? static_cast<double>(request.dry_run_speed_mm_s.value_or(0.0f))
        : static_cast<double>(request.dispensing_speed_mm_s.value_or(0.0f));
}

void LogPreviewGateFailure(
    const char* command_name,
    const std::string& request_id,
    const std::string& plan_id,
    const std::string& preview_failure_reason) {
    SILIGEN_LOG_WARNING(
        std::string(command_name) +
        " gate_failure=true request_id=" + request_id +
        " plan_id=" + plan_id +
        " preview_failure_reason=\"" + preview_failure_reason + "\"");
}

std::string ComputePreviewSnapshotHash(
    const std::string& preview_request_signature,
    uint32_t segment_count,
    uint32_t point_count,
    double total_length_mm,
    double estimated_time_s) {
    std::ostringstream seed_builder;
    seed_builder << preview_request_signature << "|" << segment_count << "|" << point_count << "|"
                 << std::fixed << std::setprecision(6) << total_length_mm << "|"
                 << std::fixed << std::setprecision(6) << estimated_time_s;
    const std::string seed = seed_builder.str();
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : seed) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return HexEncodeUint64(hash);
}

std::string TrimAscii(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> SplitCommaSeparated(const std::string& text) {
    std::vector<std::string> items;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto trimmed = TrimAscii(token);
        if (!trimmed.empty()) {
            items.push_back(trimmed);
        }
    }
    return items;
}

std::vector<std::string> ParseTags(const nlohmann::json& params) {
    std::vector<std::string> tags;
    if (!params.contains("tags")) {
        return tags;
    }
    const auto& value = params.at("tags");
    if (value.is_array()) {
        for (const auto& entry : value) {
            if (entry.is_string()) {
                auto tag = TrimAscii(entry.get<std::string>());
                if (!tag.empty()) {
                    tags.push_back(tag);
                }
            }
        }
        return tags;
    }
    if (value.is_string()) {
        return SplitCommaSeparated(value.get<std::string>());
    }
    return tags;
}

bool ParseParameterEntries(const nlohmann::json& params,
                           std::vector<ParameterValueEntry>& out,
                           std::string& error) {
    out.clear();
    if (!params.contains("parameters")) {
        return true;
    }
    const auto& entries = params.at("parameters");
    if (!entries.is_array()) {
        error = "parameters must be an array";
        return false;
    }
    for (const auto& entry_json : entries) {
        if (!entry_json.is_object()) {
            error = "parameter entry must be an object";
            return false;
        }
        auto entry_result = RecipeJsonSerializer::ParameterValueEntryFromJson(entry_json);
        if (entry_result.IsError()) {
            error = entry_result.GetError().GetMessage();
            return false;
        }
        out.push_back(entry_result.Value());
    }
    return true;
}

nlohmann::json ImportConflictToJson(const ImportConflict& conflict) {
    return {
        {"type", RecipeJsonSerializer::ImportConflictTypeToString(conflict.type)},
        {"message", conflict.message},
        {"existingId", conflict.existing_id},
        {"incomingName", conflict.incoming_name},
        {"suggestedResolution", RecipeJsonSerializer::ConflictResolutionToString(conflict.suggested_resolution)}
    };
}

std::string ToConnectionStateLabel(Siligen::Device::Contracts::State::DeviceConnectionState state) {
    using State = Siligen::Device::Contracts::State::DeviceConnectionState;
    switch (state) {
        case State::Connected:
            return "connected";
        case State::Connecting:
            return "connecting";
        case State::Error:
            return "error";
        case State::Disconnected:
        default:
            return "disconnected";
    }
}

bool IsAxisStatusHomed(const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    return status.homing_state == "homed";
}

using RuntimeStatusExportSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot;

nlohmann::json BuildRawIoJson(const RuntimeStatusExportSnapshot& snapshot) {
    return {
        {"limit_x_pos", snapshot.io.limit_x_pos},
        {"limit_x_neg", snapshot.io.limit_x_neg},
        {"limit_y_pos", snapshot.io.limit_y_pos},
        {"limit_y_neg", snapshot.io.limit_y_neg},
        {"estop", snapshot.io.estop},
        {"estop_known", snapshot.io.estop_known},
        {"door", snapshot.io.door},
        {"door_known", snapshot.io.door_known}
    };
}

nlohmann::json BuildEffectiveInterlocksJson(const RuntimeStatusExportSnapshot& snapshot) {
    return {
        {"estop_active", snapshot.effective_interlocks.estop_active},
        {"estop_known", snapshot.effective_interlocks.estop_known},
        {"door_open_active", snapshot.effective_interlocks.door_open_active},
        {"door_open_known", snapshot.effective_interlocks.door_open_known},
        {"home_boundary_x_active", snapshot.effective_interlocks.home_boundary_x_active},
        {"home_boundary_y_active", snapshot.effective_interlocks.home_boundary_y_active},
        {"positive_escape_only_axes", snapshot.effective_interlocks.positive_escape_only_axes},
        {"sources", snapshot.effective_interlocks.sources}
    };
}

nlohmann::json BuildSupervisionJson(const RuntimeStatusExportSnapshot& snapshot) {
    return {
        {"current_state", snapshot.supervision.current_state},
        {"requested_state", snapshot.supervision.requested_state},
        {"state_change_in_process", snapshot.supervision.state_change_in_process},
        {"state_reason", snapshot.supervision.state_reason},
        {"failure_code", snapshot.supervision.failure_code},
        {"failure_stage", snapshot.supervision.failure_stage},
        {"recoverable", snapshot.supervision.recoverable},
        {"updated_at", snapshot.supervision.updated_at}
    };
}

nlohmann::json BuildAxesJson(const RuntimeStatusExportSnapshot& snapshot) {
    nlohmann::json axes_json = nlohmann::json::object();
    for (const auto& [axis_name, axis_status] : snapshot.axes) {
        axes_json[axis_name] = {
            {"position", axis_status.position},
            {"velocity", axis_status.velocity},
            {"enabled", axis_status.enabled},
            {"homed", axis_status.homed},
            {"homing_state", axis_status.homing_state}
        };
    }
    return axes_json;
}

nlohmann::json BuildPositionJson(const RuntimeStatusExportSnapshot& snapshot) {
    if (!snapshot.has_position) {
        return nlohmann::json::object();
    }
    return {
        {"x", snapshot.position.x},
        {"y", snapshot.position.y}
    };
}

nlohmann::json BuildDispenserJson(const RuntimeStatusExportSnapshot& snapshot) {
    return {
        {"valve_open", snapshot.dispenser.valve_open},
        {"supply_open", snapshot.dispenser.supply_open}
    };
}

struct ManualInterlockSnapshot {
    bool emergency_stop_active = false;
    bool safety_door_open = false;
    bool interlock_latched = false;
};

ManualInterlockSnapshot ReadManualInterlockSnapshot(
    const std::shared_ptr<TcpFacades::TcpSystemFacade>& system_facade,
    const std::shared_ptr<TcpFacades::TcpDispensingFacade>& dispensing_facade) {
    ManualInterlockSnapshot snapshot;

    if (system_facade) {
        auto estop_result = system_facade->IsInEmergencyStop();
        if (estop_result.IsSuccess()) {
            snapshot.emergency_stop_active = estop_result.Value();
        }
    }

    if (dispensing_facade) {
        auto interlock_result = dispensing_facade->ReadInterlockSignals();
        if (interlock_result.IsSuccess()) {
            const auto& signals = interlock_result.Value();
            snapshot.emergency_stop_active = snapshot.emergency_stop_active || signals.emergency_stop_triggered;
            snapshot.safety_door_open = signals.safety_door_open;
        }
        snapshot.interlock_latched = dispensing_facade->IsInterlockLatched();
    }

    return snapshot;
}

std::string DescribeManualInterlock(const ManualInterlockSnapshot& snapshot) {
    if (snapshot.emergency_stop_active) {
        return "Emergency stop active";
    }
    if (snapshot.safety_door_open) {
        return "Safety door open";
    }
    if (snapshot.interlock_latched) {
        return "Interlock latched";
    }
    return "";
}

std::optional<std::string> MakeManualInterlockErrorResponse(
    const std::string& id,
    int error_code,
    const ManualInterlockSnapshot& snapshot) {
    const std::string message = DescribeManualInterlock(snapshot);
    if (message.empty()) {
        return std::nullopt;
    }
    return GatewayJsonProtocol::MakeErrorResponse(id, error_code, message);
}

}  // namespace

namespace Siligen::Adapters::Tcp {

using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Domain::Recipes::Serialization::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;

TcpCommandDispatcher::TcpCommandDispatcher(
    std::shared_ptr<Application::Facades::Tcp::TcpSystemFacade> systemFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpMotionFacade> motionFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpDispensingFacade> dispensingFacade,
    std::shared_ptr<Application::Facades::Tcp::TcpRecipeFacade> recipeFacade,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> configPort,
    std::shared_ptr<RuntimeExecution::Contracts::System::IRuntimeStatusExportPort> runtimeStatusExportPort,
    std::shared_ptr<MockIoControlService> mockIoControl)
    : systemFacade_(std::move(systemFacade))
    , motionFacade_(std::move(motionFacade))
    , dispensingFacade_(std::move(dispensingFacade))
    , recipeFacade_(std::move(recipeFacade))
    , configPort_(std::move(configPort))
    , runtimeStatusExportPort_(std::move(runtimeStatusExportPort))
    , mockIoControl_(std::move(mockIoControl))
{
    RegisterCommands();
    LoadDiagnosticsConfig();
}

void TcpCommandDispatcher::RegisterCommand(const std::string& name, CommandHandler handler) {
    commandHandlers_[name] = std::move(handler);
}

void TcpCommandDispatcher::RegisterCoreCommands() {
    RegisterCommand("ping", [this](const std::string& id, const nlohmann::json& params) { return HandlePing(id, params); });
    RegisterCommand("connect", [this](const std::string& id, const nlohmann::json& params) { return HandleConnect(id, params); });
    RegisterCommand("disconnect", [this](const std::string& id, const nlohmann::json& params) { return HandleDisconnect(id, params); });
    RegisterCommand("estop.reset", [this](const std::string& id, const nlohmann::json& params) { return HandleEstopReset(id, params); });
    RegisterCommand("mock.io.set", [this](const std::string& id, const nlohmann::json& params) { return HandleMockIoSet(id, params); });
    RegisterCommand("status", [this](const std::string& id, const nlohmann::json& params) { return HandleStatus(id, params); });
}

void TcpCommandDispatcher::RegisterMotionCommands() {
    RegisterCommand("motion.coord.status", [this](const std::string& id, const nlohmann::json& params) { return HandleMotionCoordStatus(id, params); });
    RegisterCommand("home", [this](const std::string& id, const nlohmann::json& params) { return HandleHome(id, params); });
    RegisterCommand("home.auto", [this](const std::string& id, const nlohmann::json& params) { return HandleHomeAuto(id, params); });
    RegisterCommand("home.go", [this](const std::string& id, const nlohmann::json& params) { return HandleHomeGo(id, params); });
    RegisterCommand("jog", [this](const std::string& id, const nlohmann::json& params) { return HandleJog(id, params); });
    RegisterCommand("move", [this](const std::string& id, const nlohmann::json& params) { return HandleMove(id, params); });
    RegisterCommand("stop", [this](const std::string& id, const nlohmann::json& params) { return HandleStop(id, params); });
    RegisterCommand("estop", [this](const std::string& id, const nlohmann::json& params) { return HandleEstop(id, params); });
}

void TcpCommandDispatcher::RegisterDispensingCommands() {
    RegisterCommand("dispenser.start", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserStart(id, params); });
    RegisterCommand("dispenser.stop", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserStop(id, params); });
    RegisterCommand("dispenser.pause", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserPause(id, params); });
    RegisterCommand("dispenser.resume", [this](const std::string& id, const nlohmann::json& params) { return HandleDispenserResume(id, params); });
    RegisterCommand("purge", [this](const std::string& id, const nlohmann::json& params) { return HandlePurge(id, params); });
    RegisterCommand("supply.open", [this](const std::string& id, const nlohmann::json& params) { return HandleSupplyOpen(id, params); });
    RegisterCommand("supply.close", [this](const std::string& id, const nlohmann::json& params) { return HandleSupplyClose(id, params); });
}

void TcpCommandDispatcher::RegisterDxfCommands() {
    RegisterCommand("dxf.load", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfLoad(id, params); });
    RegisterCommand("dxf.artifact.create", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfArtifactCreate(id, params); });
    RegisterCommand("dxf.plan.prepare", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfPlanPrepare(id, params); });
    RegisterCommand("dxf.job.start", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobStart(id, params); });
    RegisterCommand("dxf.job.status", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobStatus(id, params); });
    RegisterCommand("dxf.job.pause", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobPause(id, params); });
    RegisterCommand("dxf.job.resume", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobResume(id, params); });
    RegisterCommand("dxf.job.stop", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobStop(id, params); });
    RegisterCommand("dxf.job.cancel", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfJobCancel(id, params); });
    RegisterCommand("dxf.preview.snapshot", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfPreviewSnapshot(id, params); });
    RegisterCommand("dxf.preview.confirm", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfPreviewConfirm(id, params); });
    RegisterCommand("dxf.info", [this](const std::string& id, const nlohmann::json& params) { return HandleDxfInfo(id, params); });
}

void TcpCommandDispatcher::RegisterAlarmCommands() {
    RegisterCommand("alarms.list", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsList(id, params); });
    RegisterCommand("alarms.clear", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsClear(id, params); });
    RegisterCommand("alarms.acknowledge", [this](const std::string& id, const nlohmann::json& params) { return HandleAlarmsAcknowledge(id, params); });
}

void TcpCommandDispatcher::RegisterRecipeCommands() {
    RegisterCommand("recipe.list", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeList(id, params); });
    RegisterCommand("recipe.get", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeGet(id, params); });
    RegisterCommand("recipe.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeCreate(id, params); });
    RegisterCommand("recipe.update", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeUpdate(id, params); });
    RegisterCommand("recipe.archive", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeArchive(id, params); });
    RegisterCommand("recipe.draft.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeDraftCreate(id, params); });
    RegisterCommand("recipe.draft.update", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeDraftUpdate(id, params); });
    RegisterCommand("recipe.publish", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipePublish(id, params); });
    RegisterCommand("recipe.versions", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeVersions(id, params); });
    RegisterCommand("recipe.version.create", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeVersionCreate(id, params); });
    RegisterCommand("recipe.version.compare", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeCompare(id, params); });
    RegisterCommand("recipe.version.activate", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeActivate(id, params); });
    RegisterCommand("recipe.templates", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeTemplates(id, params); });
    RegisterCommand("recipe.schema.default", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeSchemaDefault(id, params); });
    RegisterCommand("recipe.audit", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeAudit(id, params); });
    RegisterCommand("recipe.export", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeExport(id, params); });
    RegisterCommand("recipe.import", [this](const std::string& id, const nlohmann::json& params) { return HandleRecipeImport(id, params); });
}

void TcpCommandDispatcher::RegisterCommands() {
    RegisterCoreCommands();
    RegisterMotionCommands();
    RegisterDispensingCommands();
    RegisterDxfCommands();
    RegisterAlarmCommands();
    RegisterRecipeCommands();
}

void TcpCommandDispatcher::LoadDiagnosticsConfig() {
    if (!configPort_) {
        return;
    }
    auto result = configPort_->GetDiagnosticsConfig();
    if (result.IsSuccess()) {
        diagnostics_config_ = result.Value();
        SILIGEN_LOG_INFO("TcpCommandDispatcher diagnostics: " + diagnostics_config_.ToString());
    } else {
        SILIGEN_LOG_WARNING("TcpCommandDispatcher diagnostics load failed: " + result.GetError().GetMessage());
    }
}

bool TcpCommandDispatcher::IsDeepTcpLoggingEnabled() const {
    return diagnostics_config_.deep_tcp_logging;
}

bool TcpCommandDispatcher::IsDeepMotionLoggingEnabled() const {
    return diagnostics_config_.deep_motion_logging;
}

std::string TcpCommandDispatcher::TruncatePayload(const std::string& payload) const {
    const size_t limit = diagnostics_config_.tcp_payload_max_chars > 0
        ? static_cast<size_t>(diagnostics_config_.tcp_payload_max_chars)
        : payload.size();
    if (payload.size() <= limit) {
        return payload;
    }
    return payload.substr(0, limit) + "...";
}

std::string TcpCommandDispatcher::Dispatch(const std::string& message) {
    if (IsDeepTcpLoggingEnabled()) {
        SILIGEN_LOG_INFO("TCP RX len=" + std::to_string(message.size()) +
                         " payload=" + TruncatePayload(message));
    }
    auto jsonOpt = GatewayJsonProtocol::Parse(message);
    if (!jsonOpt) {
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_WARNING("TCP RX parse failed: invalid JSON");
        }
        return GatewayJsonProtocol::MakeErrorResponse("", 1000, "Invalid JSON format");
    }

    const auto& json = *jsonOpt;

    // 提取请求字段
    std::string id = json.value("id", "");
    std::string method = json.value("method", "");
    nlohmann::json params = json.value("params", nlohmann::json::object());

    if (method.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 1001, "Missing 'method' field");
    }

    std::string trace_id = id.empty()
        ? Siligen::Shared::Types::TraceContext::GenerateTraceId()
        : id;
    Siligen::Shared::Types::TraceContext::SetTraceId(trace_id);
    if (!id.empty()) {
        Siligen::Shared::Types::TraceContext::SetAttribute("request_id", id);
    }

    if (IsDeepTcpLoggingEnabled()) {
        SILIGEN_LOG_INFO("TCP Dispatch method=" + method +
                         " id=" + id +
                         " params=" + TruncatePayload(params.dump()));
    }

    // 查找并执行命令处理器
    auto it = commandHandlers_.find(method);
    if (it == commandHandlers_.end()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 1002, "Unknown method: " + method);
    }

    try {
        auto response = it->second(id, params);
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_INFO("TCP TX len=" + std::to_string(response.size()) +
                             " response=" + TruncatePayload(response));
        }
        return response;
    } catch (const std::exception& e) {
        if (IsDeepTcpLoggingEnabled()) {
            SILIGEN_LOG_WARNING(std::string("TCP handler exception: ") + e.what());
        }
        return GatewayJsonProtocol::MakeErrorResponse(id, 1003, std::string("Handler error: ") + e.what());
    }
}

// ========== 命令处理实现 ==========

std::string TcpCommandDispatcher::HandlePing(const std::string& id, const nlohmann::json& /*params*/) {
    return GatewayJsonProtocol::MakePongResponse(id);
}

std::string TcpCommandDispatcher::HandleConnect(const std::string& id, const nlohmann::json& params) {
    if (!systemFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2000, "TcpSystemFacade not available");
    }

    Application::UseCases::System::InitializeSystemRequest request;
    request.auto_connect_hardware = true;

    // 从params提取连接配置
    if (params.contains("card_ip")) {
        request.connection_config.card_ip = params["card_ip"].get<std::string>();
    }
    if (params.contains("local_ip")) {
        request.connection_config.local_ip = params["local_ip"].get<std::string>();
    }

    auto result = systemFacade_->Initialize(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2001, result.GetError().GetMessage());
    }

    auto response = result.Value();
    nlohmann::json resultJson = {
        {"connected", response.hardware_connected},
        {"message", response.status_message}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDisconnect(const std::string& id, const nlohmann::json& /*params*/) {
    // 当前协议层仅提供幂等断开确认语义，真实链路回收由上层宿主与客户端自行处理。
    nlohmann::json resultJson = {{"disconnected", true}};
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleMockIoSet(const std::string& id, const nlohmann::json& params) {
    if (!mockIoControl_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2050, "mock.io.set backend not available");
    }

    MockIoUpdateRequest request;
    request.estop = ReadOptionalJsonBool(params, "estop");
    request.door = ReadOptionalJsonBool(params, "door");
    request.limit_x_neg = ReadOptionalJsonBool(params, "limit_x_neg");
    request.limit_y_neg = ReadOptionalJsonBool(params, "limit_y_neg");

    const auto result = mockIoControl_->Apply(request);
    if (result.IsError()) {
        const auto& error = result.GetError();
        int code = 2050;
        if (error.GetCode() == Shared::Types::ErrorCode::INVALID_CONFIG_VALUE) {
            code = 2051;
        } else if (error.GetCode() == Shared::Types::ErrorCode::HARDWARE_OPERATION_FAILED) {
            code = 2052;
        }
        return GatewayJsonProtocol::MakeErrorResponse(id, code, error.GetMessage());
    }

    const auto& state = result.Value();
    nlohmann::json resultJson = {
        {"estop", state.estop},
        {"door", state.door},
        {"limit_x_neg", state.limit_x_neg},
        {"limit_y_neg", state.limit_y_neg}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleStatus(const std::string& id, const nlohmann::json& /*params*/) {
    if (!runtimeStatusExportPort_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2101, "RuntimeStatusExportPort not available");
    }

    const auto status_result = runtimeStatusExportPort_->ReadSnapshot();
    if (status_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2101, status_result.GetError().GetMessage());
    }
    const auto& status_snapshot = status_result.Value();
    const nlohmann::json axesJson = BuildAxesJson(status_snapshot);
    const nlohmann::json positionJson = BuildPositionJson(status_snapshot);
    const nlohmann::json dispenserJson = BuildDispenserJson(status_snapshot);
    const nlohmann::json ioJson = BuildRawIoJson(status_snapshot);
    const nlohmann::json effectiveInterlocksJson = BuildEffectiveInterlocksJson(status_snapshot);
    const nlohmann::json supervisionJson = BuildSupervisionJson(status_snapshot);

    nlohmann::json resultJson = {
        {"connected", status_snapshot.connected},
        {"connection_state", status_snapshot.connection_state},
        {"machine_state", status_snapshot.machine_state},
        {"machine_state_reason", status_snapshot.machine_state_reason},
        {"supervision", supervisionJson},
        {"interlock_latched", status_snapshot.interlock_latched},
        {"active_job_id", status_snapshot.active_job_id},
        {"active_job_state", status_snapshot.active_job_state},
        {"axes", axesJson},
        {"position", positionJson},
        {"effective_interlocks", effectiveInterlocksJson},
        {"io", ioJson},
        {"dispenser", dispenserJson},
        {"alarms", nlohmann::json::array()}
    };

    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleHome(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2200, "TcpMotionFacade not available");
    }
    if (!motionFacade_->IsHardwareReadyForMotion()) {
        const auto heartbeat_status = motionFacade_->GetHeartbeatStatus();
        const std::string message = heartbeat_status.is_degraded
            ? "Hardware connection degraded, reconnect before homing"
            : "Hardware not connected";
        return GatewayJsonProtocol::MakeErrorResponse(id, 2201, message);
    }

    Application::UseCases::Motion::Homing::HomeAxesRequest request;

    if (params.contains("axes") && params["axes"].is_array()) {
        for (const auto& axis : params["axes"]) {
            if (!axis.is_string()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid axis name");
            }
            auto axis_id = AxisFromName(axis.get<std::string>());
            if (!IsValid(axis_id)) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid axis name");
            }
            request.axes.push_back(axis_id);
        }
        if (request.axes.empty()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Empty axis list");
        }
    } else {
        request.home_all_axes = true;
    }

    if (params.contains("force")) {
        if (!params["force"].is_boolean()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2202, "Invalid force flag");
        }
        request.force_rehome = params["force"].get<bool>();
    }

    auto result = motionFacade_->Home(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2201, result.GetError().GetMessage());
    }

    auto response = result.Value();
    auto axes_to_json = [](const std::vector<LogicalAxisId>& axes) {
        nlohmann::json values = nlohmann::json::array();
        for (const auto axis : axes) {
            values.push_back(Siligen::Shared::Types::AxisName(axis));
        }
        return values;
    };
    auto axis_results_to_json = [](const std::vector<Application::UseCases::Motion::Homing::HomeAxesResponse::AxisResult>&
                                       axis_results) {
        nlohmann::json values = nlohmann::json::array();
        for (const auto& axis_result : axis_results) {
            values.push_back({
                {"axis", Siligen::Shared::Types::AxisName(axis_result.axis)},
                {"success", axis_result.success},
                {"message", axis_result.message},
            });
        }
        return values;
    };
    nlohmann::json resultJson = {
        {"completed", response.all_completed},
        {"message", response.status_message},
        {"succeeded_axes", axes_to_json(response.successfully_homed_axes)},
        {"failed_axes", axes_to_json(response.failed_axes)},
        {"error_messages", response.error_messages},
        {"axis_results", axis_results_to_json(response.axis_results)},
        {"total_time_ms", response.total_time_ms},
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleHomeAuto(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2420, "TcpMotionFacade not available");
    }
    if (!motionFacade_->IsHardwareReadyForMotion()) {
        const auto heartbeat_status = motionFacade_->GetHeartbeatStatus();
        const std::string message = heartbeat_status.is_degraded
            ? "Hardware connection degraded, reconnect before ready-zero"
            : "Hardware not connected";
        return GatewayJsonProtocol::MakeErrorResponse(id, 2421, message);
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2423,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    auto readiness_result = motionFacade_->EvaluateMotionReadiness(BuildReadinessQuery(ResolveActiveDxfJobState()));
    if (readiness_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2424, readiness_result.GetError().GetMessage());
    }
    if (!readiness_result.Value().ready) {
        const auto reason_code = ResolveReadinessReasonCode(readiness_result.Value());
        const auto message = ResolveReadinessMessage(readiness_result.Value());
        return GatewayJsonProtocol::MakeSuccessResponse(id, {
            {"accepted", false},
            {"summary_state", "blocked"},
            {"reason_code", reason_code},
            {"message", message},
            {"axis_results", nlohmann::json::array()},
            {"total_time_ms", 0},
        });
    }

    Application::UseCases::Motion::Homing::EnsureAxesReadyZeroRequest request;
    request.home_all_axes = true;

    if (params.contains("axes")) {
        if (!params["axes"].is_array()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid axis list");
        }
        request.home_all_axes = false;
        for (const auto& axis : params["axes"]) {
            if (!axis.is_string()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid axis name");
            }
            auto axis_id = AxisFromName(axis.get<std::string>());
            if (!IsValid(axis_id)) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid axis name");
            }
            request.axes.push_back(axis_id);
        }
        if (request.axes.empty()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Empty axis list");
        }
    }

    if (params.contains("force")) {
        if (!params["force"].is_boolean()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid force flag");
        }
        request.force_rehome = params["force"].get<bool>();
    }
    if (params.contains("wait_for_completion")) {
        if (!params["wait_for_completion"].is_boolean()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid wait_for_completion flag");
        }
        request.wait_for_completion = params["wait_for_completion"].get<bool>();
    }
    if (params.contains("timeout_ms")) {
        if (!params["timeout_ms"].is_number_integer()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2422, "Invalid timeout_ms");
        }
        request.timeout_ms = params["timeout_ms"].get<int>();
    }

    auto result = motionFacade_->EnsureAxesReadyZero(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2424, result.GetError().GetMessage());
    }

    const auto& response = result.Value();
    nlohmann::json axisResultsJson = nlohmann::json::array();
    for (const auto& axis_result : response.axis_results) {
        axisResultsJson.push_back({
            {"axis", Siligen::Shared::Types::AxisName(axis_result.axis)},
            {"supervisor_state", axis_result.supervisor_state},
            {"planned_action", axis_result.planned_action},
            {"executed", axis_result.executed},
            {"success", axis_result.success},
            {"reason_code", axis_result.reason_code},
            {"message", axis_result.message},
        });
    }

    nlohmann::json result_json = {
        {"accepted", response.accepted},
        {"summary_state", response.summary_state},
        {"reason_code", response.reason_code},
        {"message", response.message},
        {"axis_results", axisResultsJson},
        {"total_time_ms", response.total_time_ms},
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, result_json);
}

std::string TcpCommandDispatcher::HandleHomeGo(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2410, "TcpMotionFacade not available");
    }
    if (!motionFacade_->IsHardwareReadyForMotion()) {
        const auto heartbeat_status = motionFacade_->GetHeartbeatStatus();
        const std::string message = heartbeat_status.is_degraded
            ? "Hardware connection degraded, reconnect before go-home"
            : "Hardware not connected";
        return GatewayJsonProtocol::MakeErrorResponse(id, 2411, message);
    }

    std::vector<LogicalAxisId> axes;
    if (params.contains("axes") && params["axes"].is_array()) {
        for (const auto& axis : params["axes"]) {
            if (!axis.is_string()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Invalid axis name");
            }
            auto axis_id = AxisFromName(axis.get<std::string>());
            if (!IsValid(axis_id)) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Invalid axis name");
            }
            axes.push_back(axis_id);
        }
        if (axes.empty()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2412, "Empty axis list");
        }
    } else {
        int axis_count = 2;
        if (configPort_) {
            auto config_result = configPort_->GetHardwareConfiguration();
            if (config_result.IsSuccess()) {
                axis_count = static_cast<int>(config_result.Value().EffectiveAxisCount());
            }
        }
        for (int axis = 0; axis < axis_count; ++axis) {
            auto axis_id = FromIndex(static_cast<int16>(axis));
            if (IsValid(axis_id)) {
                axes.push_back(axis_id);
            }
        }
    }

    for (auto axis_id : axes) {
        auto homed_result = motionFacade_->IsAxisHomed(axis_id);
        if (homed_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2413, homed_result.GetError().GetMessage());
        }
        if (!homed_result.Value()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2414, "Axis not homed, run homing first");
        }
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2417,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    auto readiness_result = motionFacade_->EvaluateMotionReadiness(BuildReadinessQuery(ResolveActiveDxfJobState()));
    if (readiness_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2416, readiness_result.GetError().GetMessage());
    }
    if (!readiness_result.Value().ready) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2416, ResolveReadinessMessage(readiness_result.Value()));
    }

    if (params.contains("speed")) {
        return GatewayJsonProtocol::MakeErrorResponse(
            id,
            2415,
            "home.go speed override is not supported; configure ready_zero_speed_mm_s");
    }

    std::vector<std::pair<LogicalAxisId, float32>> go_home_plans;
    go_home_plans.reserve(axes.size());
    for (auto axis_id : axes) {
        auto speed_result = Siligen::Domain::Configuration::Services::ResolveReadyZeroSpeed(
            axis_id,
            configPort_,
            "TcpCommandDispatcher");
        if (speed_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2415, speed_result.GetError().GetMessage());
        }
        go_home_plans.emplace_back(axis_id, speed_result.Value().speed_mm_s);
    }

    for (const auto& plan : go_home_plans) {
        Application::UseCases::Motion::Manual::ManualMotionCommand cmd;
        cmd.axis = plan.first;
        cmd.position = 0.0f;
        cmd.velocity = plan.second;
        auto result = motionFacade_->ExecutePointToPointMotion(cmd, false);
        if (!result.IsSuccess()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2416, result.GetError().GetMessage());
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"moving", true}, {"message", "Go home started"}});
}

std::string TcpCommandDispatcher::HandleJog(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2300, "TcpMotionFacade not available");
    }

    // 解析参数
    std::string axisName = params.value("axis", "X");
    int direction = params.value("direction", 1);
    std::optional<float> speed_override;
    if (params.contains("speed")) {
        speed_override = params.value("speed", 0.0f);
    }

    auto axis_id = AxisFromName(axisName);
    if (!IsValid(axis_id)) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2302, "Invalid axis name");
    }

    auto axis_status_result = motionFacade_->GetAxisMotionStatus(axis_id);
    if (axis_status_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2304, axis_status_result.GetError().GetMessage());
    }
    const auto& axis_status = axis_status_result.Value();
    const bool is_homed = IsAxisStatusHomed(axis_status);
    const bool positive_escape_from_home = axis_status.home_signal && direction > 0;

    if (axis_status.state == Siligen::Domain::Motion::Ports::MotionState::HOMING) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2304, "Axis is homing, wait for completion first");
    }
    if (!is_homed && !positive_escape_from_home) {
        const std::string message = axis_status.home_signal
            ? "Axis not homed; HOME is active, only positive escape jog is allowed"
            : "Axis not homed, run homing first";
        return GatewayJsonProtocol::MakeErrorResponse(id, 2304, message);
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2305,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    float speed = 0.0f;
    if (speed_override.has_value()) {
        speed = speed_override.value();
    } else {
        if (!configPort_) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, "Config port not available");
        }
        auto machine_result = configPort_->GetMachineConfig();
        if (machine_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, machine_result.GetError().GetMessage());
        }
        speed = machine_result.Value().max_speed;
    }
    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Jog deep params: axis=" + axisName +
                         " dir=" + std::to_string(direction) +
                         " speed=" + std::to_string(speed) +
                         " params=" + TruncatePayload(params.dump()));
        LogAxisSnapshot("Jog deep pre", motionFacade_, axis_id);
    }

    SILIGEN_LOG_INFO_FMT_HELPER("Jog request: axis=%s dir=%d speed=%.3f",
                                axisName.c_str(),
                                direction,
                                speed);
    LogAxisSnapshot("Jog pre", motionFacade_, axis_id);

    auto result = motionFacade_->StartJog(axis_id, static_cast<int16>(direction), speed);
    if (!result.IsSuccess()) {
        const auto& error = result.GetError();
        if (error.GetCode() == Siligen::Shared::Types::ErrorCode::VELOCITY_LIMIT_EXCEEDED) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2303, "Invalid jog speed");
        }
        return GatewayJsonProtocol::MakeErrorResponse(id, 2301, error.GetMessage());
    }

    LogAxisSnapshot("Jog post", motionFacade_, axis_id);
    if (IsDeepMotionLoggingEnabled() && diagnostics_config_.snapshot_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_delay_ms));
        LogAxisSnapshot("Jog deep delayed", motionFacade_, axis_id);
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"jogging", true}});
}

std::string TcpCommandDispatcher::HandleMove(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2400, "TcpMotionFacade not available");
    }

    // 解析目标位置
    float x = params.value("x", 0.0f);
    float y = params.value("y", 0.0f);
    std::optional<float> speed_override;
    if (params.contains("speed")) {
        speed_override = params.value("speed", 0.0f);
    }

    float speed = 0.0f;
    if (speed_override.has_value()) {
        speed = speed_override.value();
    } else {
        if (!configPort_) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2402, "Config port not available");
        }
        auto machine_result = configPort_->GetMachineConfig();
        if (machine_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2402, machine_result.GetError().GetMessage());
        }
        speed = machine_result.Value().max_speed;
    }
    if (speed <= 0.0f) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2402, "Invalid move speed");
    }

    for (const auto axis_id : {LogicalAxisId::X, LogicalAxisId::Y}) {
        auto homed_result = motionFacade_->IsAxisHomed(axis_id);
        if (homed_result.IsError()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2403, homed_result.GetError().GetMessage());
        }
        if (!homed_result.Value()) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 2403, "Axis not homed, run homing first");
        }
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2404,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    auto readiness_result = motionFacade_->EvaluateMotionReadiness(BuildReadinessQuery(ResolveActiveDxfJobState()));
    if (readiness_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, readiness_result.GetError().GetMessage());
    }
    if (!readiness_result.Value().ready) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, ResolveReadinessMessage(readiness_result.Value()));
    }

    auto current_position_result = motionFacade_->GetCurrentPosition();
    if (current_position_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, current_position_result.GetError().GetMessage());
    }
    const auto current_position = current_position_result.Value();

    if (auto x_limit_error = CheckMoveAxisLimitBeforeDispatch(motionFacade_, LogicalAxisId::X, current_position.x, x);
        x_limit_error.has_value()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, x_limit_error.value());
    }
    if (auto y_limit_error = CheckMoveAxisLimitBeforeDispatch(motionFacade_, LogicalAxisId::Y, current_position.y, y);
        y_limit_error.has_value()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, y_limit_error.value());
    }

    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Move deep params: x=" + std::to_string(x) +
                         " y=" + std::to_string(y) +
                         " speed=" + std::to_string(speed) +
                         " params=" + TruncatePayload(params.dump()));
        LogAxisSnapshot("Move deep pre", motionFacade_, LogicalAxisId::X);
        LogAxisSnapshot("Move deep pre", motionFacade_, LogicalAxisId::Y);
    }

    // 执行X轴移动
    Application::UseCases::Motion::Manual::ManualMotionCommand cmdX;
    cmdX.axis = LogicalAxisId::X;
    cmdX.position = x;
    cmdX.velocity = speed;
    auto resultX = motionFacade_->ExecutePointToPointMotion(cmdX);

    // 执行Y轴移动
    Application::UseCases::Motion::Manual::ManualMotionCommand cmdY;
    cmdY.axis = LogicalAxisId::Y;
    cmdY.position = y;
    cmdY.velocity = speed;
    auto resultY = motionFacade_->ExecutePointToPointMotion(cmdY);

    if (!resultX.IsSuccess() || !resultY.IsSuccess()) {
        std::string errMsg = resultX.IsSuccess() ? resultY.GetError().GetMessage() : resultX.GetError().GetMessage();
        return GatewayJsonProtocol::MakeErrorResponse(id, 2401, errMsg);
    }

    if (IsDeepMotionLoggingEnabled()) {
        LogAxisSnapshot("Move deep post", motionFacade_, LogicalAxisId::X);
        LogAxisSnapshot("Move deep post", motionFacade_, LogicalAxisId::Y);
        if (diagnostics_config_.snapshot_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_delay_ms));
            LogAxisSnapshot("Move deep delayed", motionFacade_, LogicalAxisId::X);
            LogAxisSnapshot("Move deep delayed", motionFacade_, LogicalAxisId::Y);
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"moving", true}});
}

std::string TcpCommandDispatcher::HandleStop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2500, "TcpMotionFacade not available");
    }

    SILIGEN_LOG_INFO("Stop request received");
    if (IsDeepMotionLoggingEnabled()) {
        SILIGEN_LOG_INFO("Stop deep request received");
    }

    const std::vector<LogicalAxisId> axes_to_log = {LogicalAxisId::X, LogicalAxisId::Y};
    for (const auto axis_id : axes_to_log) {
        LogAxisSnapshot("Stop pre", motionFacade_, axis_id);
    }

    auto stop_result = motionFacade_->StopAllAxes(false);
    if (!stop_result.IsSuccess()) {
        SILIGEN_LOG_WARNING_FMT_HELPER("Stop all axes failed reason=%s", stop_result.GetError().GetMessage().c_str());
        return GatewayJsonProtocol::MakeErrorResponse(id, 2500, stop_result.GetError().GetMessage());
    }

    for (const auto axis_id : axes_to_log) {
        LogAxisSnapshot("Stop post", motionFacade_, axis_id);
        if (IsDeepMotionLoggingEnabled() && diagnostics_config_.snapshot_after_stop_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(diagnostics_config_.snapshot_after_stop_ms));
            LogAxisSnapshot("Stop deep delayed", motionFacade_, axis_id);
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"stopped", true}});
}

std::string TcpCommandDispatcher::HandleEstop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!systemFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2600, "TcpSystemFacade not available");
    }

    Application::UseCases::System::EmergencyStopRequest request;
    request.reason = Application::UseCases::System::EmergencyStopReason::USER_REQUEST;
    request.detail_message = "HMI emergency stop request";

    auto result = systemFacade_->EmergencyStop(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2601, result.GetError().GetMessage());
    }

    auto response = result.Value();
    nlohmann::json resultJson = {
        {"motion_stopped", response.motion_stopped},
        {"message", response.status_message}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleEstopReset(const std::string& id, const nlohmann::json& /*params*/) {
    if (!systemFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2602, "TcpSystemFacade not available");
    }

    auto result = systemFacade_->RecoverFromEmergencyStop();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2603, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"reset", true}, {"message", "Emergency stop reset"}});
}

std::string TcpCommandDispatcher::HandleDispenserStart(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2700, "TcpDispensingFacade not available");
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2703,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    // 解析点胶参数
    Domain::Dispensing::Ports::DispenserValveParams valveParams;
    valveParams.count = params.value("count", 1u);
    valveParams.intervalMs = params.value("interval_ms", 1000u);
    valveParams.durationMs = params.value("duration_ms", 100u);

    // 验证参数
    if (!valveParams.IsValid()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2702, valveParams.GetValidationError());
    }

    auto result = dispensingFacade_->StartDispenser(valveParams);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2701, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"dispensing", true}});
}

std::string TcpCommandDispatcher::HandleDispenserStop(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2800, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->StopDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2801, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"dispensing", false}});
}

std::string TcpCommandDispatcher::HandleDispenserPause(const std::string& id, const nlohmann::json& /*params*/) {
    // 安全检查：如果有DXF作业正在运行，强制停止它（因为目前DXF不支持暂停）
    std::string current_dxf_job_id;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        current_dxf_job_id = active_dxf_job_id_;
    }

    if (!current_dxf_job_id.empty()) {
        SILIGEN_LOG_WARNING("收到暂停请求，但检测到DXF作业正在运行且不支持暂停。执行强制停止以确保安全。");
        if (dispensingFacade_) {
            auto stop_result = dispensingFacade_->StopDxfJob(current_dxf_job_id);
            if (stop_result.IsError()) {
                return GatewayJsonProtocol::MakeErrorResponse(id, 2811, stop_result.GetError().GetMessage());
            }
        }
    }

    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2810, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->PauseDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2811, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"paused", true}});
}

std::string TcpCommandDispatcher::HandleDispenserResume(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2820, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->ResumeDispenser();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2821, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"resumed", true}});
}

std::string TcpCommandDispatcher::HandlePurge(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2830, "TcpDispensingFacade not available");
    }

    // 解析参数
    int timeoutMs = params.value("timeout_ms", 5000);

    Application::UseCases::Dispensing::Valve::PurgeDispenserRequest request;
    request.manage_supply = true;
    request.wait_for_completion = true;
    request.timeout_ms = static_cast<uint32>(timeoutMs);

    auto result = dispensingFacade_->PurgeDispenser(request);
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2831, result.GetError().GetMessage());
    }

    auto response = result.Value();
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"purged", response.completed}, {"timeout_ms", timeoutMs}});
}

std::string TcpCommandDispatcher::HandleSupplyOpen(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2840, "TcpDispensingFacade not available");
    }

    if (auto interlock_error = MakeManualInterlockErrorResponse(
            id,
            2842,
            ReadManualInterlockSnapshot(systemFacade_, dispensingFacade_));
        interlock_error.has_value()) {
        return interlock_error.value();
    }

    auto result = dispensingFacade_->OpenSupplyValve();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2841, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"supply_open", true}});
}

std::string TcpCommandDispatcher::HandleSupplyClose(const std::string& id, const nlohmann::json& /*params*/) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2850, "TcpDispensingFacade not available");
    }

    auto result = dispensingFacade_->CloseSupplyValve();
    if (!result.IsSuccess()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2851, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"supply_open", false}});
}

std::string TcpCommandDispatcher::HandleDxfArtifactCreate(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2890, "TcpDispensingFacade not available");
    }

    std::string filename = params.value("filename", "");
    if (filename.empty()) {
        filename = params.value("original_filename", "");
    }
    const std::string encoded = params.value("file_content_b64", "");
    if (filename.empty() || encoded.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2891, "Missing filename or file_content_b64");
    }

    std::vector<uint8_t> decoded;
    try {
        decoded = Siligen::EncodingCodec::Base64Decode(encoded);
    } catch (...) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2892, "Invalid file_content_b64");
    }
    if (decoded.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2892, "Decoded file content is empty");
    }

    Siligen::JobIngest::Contracts::UploadRequest request;
    request.file_content = std::move(decoded);
    request.original_filename = filename;
    request.file_size = request.file_content.size();
    request.content_type = params.value("content_type", "application/dxf");

    auto artifact_result = dispensingFacade_->CreateDxfArtifact(request);
    if (artifact_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2893, artifact_result.GetError().GetMessage());
    }

    const auto& artifact = artifact_result.Value();
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_cache_ = DxfCache{};
        dxf_cache_.loaded = true;
        dxf_cache_.artifact_id = artifact.artifact_id;
        dxf_cache_.filepath = artifact.filepath;
        active_dxf_job_id_.clear();
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"created", true},
        {"artifact_id", artifact.artifact_id},
        {"filepath", artifact.filepath},
        {"original_name", artifact.original_name},
        {"generated_filename", artifact.generated_filename},
        {"size", artifact.size},
        {"timestamp", artifact.timestamp}
    });
}

std::string TcpCommandDispatcher::HandleDxfPlanPrepare(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2894, "TcpDispensingFacade not available");
    }

    std::string artifact_id = params.value("artifact_id", "");
    if (artifact_id.empty()) {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        artifact_id = dxf_cache_.artifact_id;
    }
    if (artifact_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2895, "Missing artifact_id");
    }

    const auto prepare_request = BuildPreparePlanRequest(artifact_id, "", params);
    const double effective_speed = ResolveEffectivePreviewSpeed(prepare_request.runtime_overrides);
    if (effective_speed <= 0.0) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2896, "Missing dispensing_speed_mm_s");
    }

    auto plan_result = dispensingFacade_->PrepareDxfPlan(prepare_request);
    if (plan_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2897, plan_result.GetError().GetMessage());
    }

    const auto& plan = plan_result.Value();
    const std::string request_signature = ComputePreviewRequestSignature(plan.filepath, params);
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_cache_.loaded = true;
        dxf_cache_.artifact_id = artifact_id;
        dxf_cache_.filepath = plan.filepath;
        dxf_cache_.segment_count = plan.segment_count;
        dxf_cache_.total_length = plan.total_length_mm;
        dxf_cache_.plan_id = plan.plan_id;
        dxf_cache_.plan_fingerprint = plan.plan_fingerprint;
        dxf_cache_.preview_snapshot_id.clear();
        dxf_cache_.preview_snapshot_hash.clear();
        dxf_cache_.preview_request_signature = request_signature;
        dxf_cache_.preview_generated_at.clear();
        dxf_cache_.preview_confirmed_at.clear();
        dxf_cache_.preview_state = "prepared";
        dxf_cache_.preview_source.clear();
        dxf_cache_.preview_speed_mm_s = effective_speed;
        active_dxf_job_id_.clear();
    }

    nlohmann::json performance_profile = {
        {"authority_cache_hit", plan.performance_profile.authority_cache_hit},
        {"authority_joined_inflight", plan.performance_profile.authority_joined_inflight},
        {"authority_wait_ms", plan.performance_profile.authority_wait_ms},
        {"pb_prepare_ms", plan.performance_profile.pb_prepare_ms},
        {"path_load_ms", plan.performance_profile.path_load_ms},
        {"process_path_ms", plan.performance_profile.process_path_ms},
        {"authority_build_ms", plan.performance_profile.authority_build_ms},
        {"authority_total_ms", plan.performance_profile.authority_total_ms},
        {"prepare_total_ms", plan.performance_profile.prepare_total_ms}
    };

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"plan_id", plan.plan_id},
        {"plan_fingerprint", plan.plan_fingerprint},
        {"artifact_id", artifact_id},
        {"segment_count", plan.segment_count},
        {"point_count", plan.point_count},
        {"total_length_mm", plan.total_length_mm},
        {"estimated_time_s", plan.estimated_time_s},
        {"generated_at", plan.generated_at},
        {"snapshot_id", ""},
        {"snapshot_hash", ""},
        {"preview_request_signature", request_signature},
        {"preview_state", "prepared"},
        {"performance_profile", performance_profile}
    });
}

std::string TcpCommandDispatcher::HandleDxfJobStart(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2898, "TcpDispensingFacade not available");
    }

    std::string plan_id = params.value("plan_id", "");
    std::string expected_plan_fingerprint = params.value("plan_fingerprint", "");
    std::string cached_plan_id;
    std::string cached_plan_fingerprint;
    if (plan_id.empty() || expected_plan_fingerprint.empty()) {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        cached_plan_id = dxf_cache_.plan_id;
        cached_plan_fingerprint = dxf_cache_.plan_fingerprint;
        if (plan_id.empty()) {
            plan_id = cached_plan_id;
        }
        if (expected_plan_fingerprint.empty()) {
            expected_plan_fingerprint = cached_plan_fingerprint;
        }
    } else {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        cached_plan_id = dxf_cache_.plan_id;
        cached_plan_fingerprint = dxf_cache_.plan_fingerprint;
    }
    if (plan_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2899, "Missing plan_id");
    }
    if (expected_plan_fingerprint.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2900, "Missing plan_fingerprint");
    }
    if (!cached_plan_id.empty() && !cached_plan_fingerprint.empty() &&
        plan_id == cached_plan_id && expected_plan_fingerprint != cached_plan_fingerprint) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2900, "Plan fingerprint mismatch");
    }

    const std::uint32_t target_count = static_cast<std::uint32_t>(ReadJsonSizeT(params, "target_count", 1));
    Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = plan_id;
    request.plan_fingerprint = expected_plan_fingerprint;
    request.target_count = std::max<std::uint32_t>(1, target_count);
    auto start_result = dispensingFacade_->StartDxfJob(request);
    if (start_result.IsError()) {
        LogPreviewGateFailure("dxf.job.start", id, plan_id, start_result.GetError().GetMessage());
        return GatewayJsonProtocol::MakeErrorResponse(id, 2901, start_result.GetError().GetMessage());
    }

    const auto& start_response = start_result.Value();
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        active_dxf_job_id_ = start_response.job_id;
    }

    nlohmann::json performance_profile = {
        {"execution_cache_hit", start_response.performance_profile.execution_cache_hit},
        {"execution_joined_inflight", start_response.performance_profile.execution_joined_inflight},
        {"execution_wait_ms", start_response.performance_profile.execution_wait_ms},
        {"motion_plan_ms", start_response.performance_profile.motion_plan_ms},
        {"assembly_ms", start_response.performance_profile.assembly_ms},
        {"export_ms", start_response.performance_profile.export_ms},
        {"execution_total_ms", start_response.performance_profile.execution_total_ms}
    };

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"started", start_response.started},
        {"job_id", start_response.job_id},
        {"plan_id", start_response.plan_id},
        {"plan_fingerprint", start_response.plan_fingerprint},
        {"target_count", start_response.target_count},
        {"performance_profile", performance_profile}
    });
}

std::string TcpCommandDispatcher::HandleDxfJobStatus(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2902, "TcpDispensingFacade not available");
    }

    std::string job_id = params.value("job_id", "");
    if (job_id.empty()) {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        job_id = active_dxf_job_id_;
    }
    if (job_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2903, "Missing job_id");
    }

    auto status_result = dispensingFacade_->GetDxfJobStatus(job_id);
    if (status_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2904, status_result.GetError().GetMessage());
    }
    const auto& status = status_result.Value();

    if (status.state == "completed" || status.state == "failed" || status.state == "cancelled") {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (active_dxf_job_id_ == job_id) {
            active_dxf_job_id_.clear();
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"job_id", status.job_id},
        {"plan_id", status.plan_id},
        {"plan_fingerprint", status.plan_fingerprint},
        {"state", status.state},
        {"target_count", status.target_count},
        {"completed_count", status.completed_count},
        {"current_cycle", status.current_cycle},
        {"current_segment", status.current_segment},
        {"total_segments", status.total_segments},
        {"cycle_progress_percent", status.cycle_progress_percent},
        {"overall_progress_percent", status.overall_progress_percent},
        {"elapsed_seconds", status.elapsed_seconds},
        {"error_message", status.error_message},
        {"dry_run", status.dry_run}
    });
}

std::string TcpCommandDispatcher::HandleDxfJobPause(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2905, "TcpDispensingFacade not available");
    }
    std::string job_id = params.value("job_id", "");
    if (job_id.empty()) {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        job_id = active_dxf_job_id_;
    }
    if (job_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2906, "DXF job not running");
    }
    auto pause_result = dispensingFacade_->PauseDxfJob(job_id);
    if (pause_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2907, pause_result.GetError().GetMessage());
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"paused", true}, {"job_id", job_id}});
}

std::string TcpCommandDispatcher::HandleDxfJobResume(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2908, "TcpDispensingFacade not available");
    }
    std::string job_id = params.value("job_id", "");
    if (job_id.empty()) {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        job_id = active_dxf_job_id_;
    }
    if (job_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2909, "DXF job not paused");
    }
    auto resume_result = dispensingFacade_->ResumeDxfJob(job_id);
    if (resume_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2910, resume_result.GetError().GetMessage());
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"resumed", true}, {"job_id", job_id}});
}

std::string TcpCommandDispatcher::HandleDxfJobStop(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2911, "TcpDispensingFacade not available");
    }
    std::string job_id = params.value("job_id", "");
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (job_id.empty()) {
            job_id = active_dxf_job_id_;
        }
    }
    if (job_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2912, "DXF job not running");
    }
    auto stop_result = dispensingFacade_->StopDxfJob(job_id);
    if (stop_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2913, stop_result.GetError().GetMessage());
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"stopped", true},
        {"job_id", job_id},
        {"transition_state", Application::Services::Motion::Execution::ToString(
                                 Application::Services::Motion::Execution::ExecutionTransitionState::STOPPING)},
    });
}

std::string TcpCommandDispatcher::HandleDxfJobCancel(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2914, "TcpDispensingFacade not available");
    }
    std::string job_id = params.value("job_id", "");
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (job_id.empty()) {
            job_id = active_dxf_job_id_;
        }
    }
    if (job_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2915, "DXF job not running");
    }
    auto cancel_result = dispensingFacade_->StopDxfJob(job_id);
    if (cancel_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2916, cancel_result.GetError().GetMessage());
    }
    return GatewayJsonProtocol::MakeSuccessResponse(
        id,
        {{"cancelled", true},
         {"job_id", job_id},
         {"transition_state", Application::Services::Motion::Execution::ToString(
                                  Application::Services::Motion::Execution::ExecutionTransitionState::CANCELING)}});
}

std::string TcpCommandDispatcher::HandleDxfLoad(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2900, "TcpDispensingFacade not available");
    }

    std::string active_job_id;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        active_job_id = active_dxf_job_id_;
    }
    if (!active_job_id.empty()) {
        auto stop_result = dispensingFacade_->StopDxfJob(active_job_id);
        if (stop_result.IsError()) {
            SILIGEN_LOG_WARNING("DXF加载前停止运行中作业失败: " + stop_result.GetError().GetMessage());
            return GatewayJsonProtocol::MakeErrorResponse(id, 2903, stop_result.GetError().GetMessage());
        }
    }

    std::string filePath = params.value("filepath", "");
    if (filePath.empty()) {
        filePath = params.value("file_path", "");
    }
    if (filePath.empty()) {
        SILIGEN_LOG_WARNING("DXF加载失败: 缺少filepath参数");
        return GatewayJsonProtocol::MakeErrorResponse(id, 2901, "Missing 'filepath' parameter");
    }
    SILIGEN_LOG_INFO("收到DXF加载请求: " + filePath);

    std::vector<uint8_t> fileContent;
    std::string readError;
    if (!ReadFileToBuffer(filePath, fileContent, readError)) {
        SILIGEN_LOG_WARNING("DXF读取失败: " + readError);
        return GatewayJsonProtocol::MakeErrorResponse(id, 2902, readError);
    }

    Siligen::JobIngest::Contracts::UploadRequest request;
    request.file_content = std::move(fileContent);
    request.original_filename = ExtractFilename(filePath);
    request.file_size = request.file_content.size();
    request.content_type = "application/dxf";

    auto uploadResult = dispensingFacade_->CreateDxfArtifact(request);
    if (!uploadResult.IsSuccess()) {
        SILIGEN_LOG_WARNING("DXF上传失败: " + uploadResult.GetError().GetMessage());
        return GatewayJsonProtocol::MakeErrorResponse(id, 2903, uploadResult.GetError().GetMessage());
    }

    auto uploadResponse = uploadResult.Value();
    DxfCache cache;
    cache.loaded = true;
    cache.artifact_id = uploadResponse.artifact_id;
    cache.filepath = uploadResponse.filepath;

    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_cache_ = cache;
        active_dxf_job_id_.clear();
    }
    SILIGEN_LOG_INFO("DXF加载成功: segments=" + std::to_string(cache.segment_count) +
                     ", filepath=" + cache.filepath);
    nlohmann::json resultJson = {
        {"loaded", true},
        {"artifact_id", cache.artifact_id},
        {"segment_count", cache.segment_count},
        {"filepath", cache.filepath}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDxfInfo(const std::string& id, const nlohmann::json& /*params*/) {
    DxfCache cache;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        cache = dxf_cache_;
    }

    if (!cache.loaded || cache.filepath.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3010, "DXF not loaded");
    }

    nlohmann::json boundsJson = {
        {"x_min", cache.x_min},
        {"x_max", cache.x_max},
        {"y_min", cache.y_min},
        {"y_max", cache.y_max}
    };

    nlohmann::json resultJson = {
        {"total_length", cache.total_length},
        {"total_segments", cache.segment_count},
        {"bounds", boundsJson}
    };
    return GatewayJsonProtocol::MakeSuccessResponse(id, resultJson);
}

std::string TcpCommandDispatcher::HandleDxfPreviewSnapshot(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3010, "TcpDispensingFacade not available");
    }

    std::string plan_id = params.value("plan_id", "");
    if (plan_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3011, "Missing plan_id");
    }

    std::string artifact_id;
    std::string filepath;
    double effective_speed = 0.0;
    std::string request_signature;

    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (dxf_cache_.plan_id == plan_id) {
            artifact_id = dxf_cache_.artifact_id;
            filepath = dxf_cache_.filepath;
            request_signature = dxf_cache_.preview_request_signature;
            effective_speed = dxf_cache_.preview_speed_mm_s;
        }
    }

    Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = plan_id;
    const std::size_t requested_polyline_points =
        ReadJsonSizeT(params, "max_polyline_points", kPreviewPolylineMaxPoints);
    if (requested_polyline_points > kPreviewPolylineMaxPoints) {
        SILIGEN_LOG_WARNING(
            "dxf.preview.snapshot max_polyline_points 超过上限，已夹断。request_id=" + id +
            ", requested=" + std::to_string(requested_polyline_points) +
            ", capped=" + std::to_string(kPreviewPolylineMaxPoints));
    }
    snapshot_request.max_polyline_points = std::min<std::size_t>(
        kPreviewPolylineMaxPoints,
        std::max<std::size_t>(2, requested_polyline_points));
    const std::size_t requested_glue_points =
        ReadJsonSizeT(params, "max_glue_points", kPreviewGlueMaxPoints);
    if (requested_glue_points > kPreviewGlueMaxPoints) {
        SILIGEN_LOG_WARNING(
            "dxf.preview.snapshot max_glue_points 超过上限，已夹断。request_id=" + id +
            ", requested=" + std::to_string(requested_glue_points) +
            ", capped=" + std::to_string(kPreviewGlueMaxPoints));
    }
    snapshot_request.max_glue_points = std::min<std::size_t>(
        kPreviewGlueMaxPoints,
        std::max<std::size_t>(1, requested_glue_points));
    auto snapshot_result = dispensingFacade_->GetDxfPreviewSnapshot(snapshot_request);
    if (snapshot_result.IsError()) {
        LogPreviewGateFailure("dxf.preview.snapshot", id, plan_id, snapshot_result.GetError().GetMessage());
        return GatewayJsonProtocol::MakeErrorResponse(id, 3012, snapshot_result.GetError().GetMessage());
    }
    const auto& snapshot = snapshot_result.Value();
    const std::string resolved_plan_id = snapshot.plan_id.empty() ? plan_id : snapshot.plan_id;
    if (resolved_plan_id != plan_id) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3012, "Preview plan_id mismatch");
    }
    if (snapshot.snapshot_hash.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview snapshot hash is missing");
    }
    if (snapshot.preview_source.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview source is missing");
    }
    if (snapshot.preview_source != "planned_glue_snapshot") {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview source must be planned_glue_snapshot");
    }
    if (snapshot.preview_kind != "glue_points") {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview kind must be glue_points");
    }

    nlohmann::json glue_points = nlohmann::json::array();
    for (const auto& point : snapshot.glue_points) {
        glue_points.push_back(BuildPreviewPointJson(point.x, point.y));
    }
    if (glue_points.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3014, "Preview glue points are empty");
    }
    nlohmann::json glue_reveal_lengths_mm = nlohmann::json::array();
    for (const auto reveal_length_mm : snapshot.glue_reveal_lengths_mm) {
        glue_reveal_lengths_mm.push_back(reveal_length_mm);
    }

    nlohmann::json motion_preview_polyline = nlohmann::json::array();
    for (const auto& point : snapshot.motion_preview_polyline) {
        motion_preview_polyline.push_back(BuildPreviewPointJson(point.x, point.y));
    }
    nlohmann::json motion_preview = {
        {"source", snapshot.motion_preview_source},
        {"kind", snapshot.motion_preview_kind},
        {"source_point_count", snapshot.motion_preview_source_point_count},
        {"point_count", snapshot.motion_preview_point_count},
        {"is_sampled", snapshot.motion_preview_is_sampled},
        {"sampling_strategy", snapshot.motion_preview_sampling_strategy},
        {"polyline", motion_preview_polyline}
    };

    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        dxf_cache_.loaded = true;
        if (!artifact_id.empty()) {
            dxf_cache_.artifact_id = artifact_id;
        }
        if (!filepath.empty()) {
            dxf_cache_.filepath = filepath;
        }
        dxf_cache_.segment_count = snapshot.segment_count;
        dxf_cache_.total_length = snapshot.total_length_mm;
        dxf_cache_.plan_id = resolved_plan_id;
        dxf_cache_.plan_fingerprint = snapshot.snapshot_hash;
        dxf_cache_.preview_snapshot_id = snapshot.snapshot_id;
        dxf_cache_.preview_snapshot_hash = snapshot.snapshot_hash;
        dxf_cache_.preview_generated_at = snapshot.generated_at;
        dxf_cache_.preview_confirmed_at = snapshot.confirmed_at;
        dxf_cache_.preview_state = snapshot.preview_state;
        dxf_cache_.preview_source = snapshot.preview_source;
        if (!request_signature.empty()) {
            dxf_cache_.preview_request_signature = request_signature;
        }
        if (effective_speed > 0.0) {
            dxf_cache_.preview_speed_mm_s = effective_speed;
        }
        active_dxf_job_id_.clear();
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"snapshot_id", snapshot.snapshot_id},
        {"snapshot_hash", snapshot.snapshot_hash},
        {"plan_id", resolved_plan_id},
        {"preview_state", snapshot.preview_state},
        {"preview_source", snapshot.preview_source},
        {"preview_kind", snapshot.preview_kind},
        {"confirmed_at", snapshot.confirmed_at},
        {"segment_count", snapshot.segment_count},
        {"point_count", snapshot.point_count},
        {"glue_point_count", snapshot.glue_point_count},
        {"glue_points", glue_points},
        {"glue_reveal_lengths_mm", glue_reveal_lengths_mm},
        {"motion_preview", motion_preview},
        {"execution_point_count", snapshot.execution_point_count},
        {"total_length_mm", snapshot.total_length_mm},
        {"estimated_time_s", snapshot.estimated_time_s},
        {"generated_at", snapshot.generated_at}
    });
}

std::string TcpCommandDispatcher::HandleDxfPreviewConfirm(const std::string& id, const nlohmann::json& params) {
    if (!dispensingFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3015, "TcpDispensingFacade not available");
    }

    std::string plan_id = params.value("plan_id", "");
    std::string snapshot_hash = params.value("snapshot_hash", "");
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (plan_id.empty()) {
            plan_id = dxf_cache_.plan_id;
        }
        if (snapshot_hash.empty()) {
            snapshot_hash = dxf_cache_.preview_snapshot_hash;
        }
    }

    if (plan_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3016, "Missing plan_id");
    }
    if (snapshot_hash.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3017, "Missing 'snapshot_hash'");
    }

    Application::UseCases::Dispensing::ConfirmPreviewRequest request;
    request.plan_id = plan_id;
    request.snapshot_hash = snapshot_hash;
    auto confirm_result = dispensingFacade_->ConfirmDxfPreview(request);
    if (confirm_result.IsError()) {
        LogPreviewGateFailure("dxf.preview.confirm", id, plan_id, confirm_result.GetError().GetMessage());
        return GatewayJsonProtocol::MakeErrorResponse(id, 3018, confirm_result.GetError().GetMessage());
    }

    const auto& confirm = confirm_result.Value();
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        if (dxf_cache_.plan_id == plan_id) {
            dxf_cache_.plan_fingerprint = confirm.snapshot_hash;
            dxf_cache_.preview_snapshot_hash = confirm.snapshot_hash;
            dxf_cache_.preview_confirmed_at = confirm.confirmed_at;
            dxf_cache_.preview_state = confirm.preview_state;
        }
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"confirmed", confirm.confirmed},
        {"plan_id", confirm.plan_id},
        {"snapshot_hash", confirm.snapshot_hash},
        {"preview_state", confirm.preview_state},
        {"confirmed_at", confirm.confirmed_at}
    });
}

std::string TcpCommandDispatcher::HandleAlarmsList(const std::string& id, const nlohmann::json& /*params*/) {
    nlohmann::json alarm_list = nlohmann::json::array();

    if (motionFacade_) {
        struct AxisInfo {
            LogicalAxisId axis;
            const char* name;
        };
        const AxisInfo axes[] = {
            {LogicalAxisId::X, "X"},
            {LogicalAxisId::Y, "Y"},
            {LogicalAxisId::Z, "Z"},
            {LogicalAxisId::U, "U"}
        };

        for (const auto& axis_info : axes) {
            auto servo_result = motionFacade_->ReadServoAlarmStatus(axis_info.axis);
            if (servo_result.IsSuccess() && servo_result.Value()) {
                std::string alarm_id = std::string("SERVO_") + axis_info.name;
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "CRIT"},
                    {"message", std::string(axis_info.name) + "轴伺服报警"}
                });
            }

            auto limit_pos = motionFacade_->ReadLimitStatus(axis_info.axis, true);
            if (limit_pos.IsSuccess() && limit_pos.Value()) {
                std::string alarm_id = std::string("LIMIT_") + axis_info.name + "_POS";
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "WARN"},
                    {"message", std::string(axis_info.name) + "轴正限位触发"}
                });
            }

            auto limit_neg = motionFacade_->ReadLimitStatus(axis_info.axis, false);
            if (limit_neg.IsSuccess() && limit_neg.Value()) {
                std::string alarm_id = std::string("LIMIT_") + axis_info.name + "_NEG";
                alarm_list.push_back({
                    {"id", alarm_id},
                    {"level", "WARN"},
                    {"message", std::string(axis_info.name) + "轴负限位触发"}
                });
            }
        }
    }

    std::unordered_set<std::string> acknowledged;
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged = acknowledged_alarm_ids_;
    }

    if (!acknowledged.empty() && !alarm_list.empty()) {
        nlohmann::json filtered = nlohmann::json::array();
        for (const auto& alarm : alarm_list) {
            auto id_iter = alarm.find("id");
            if (id_iter == alarm.end()) {
                continue;
            }
            const std::string alarm_id = id_iter->get<std::string>();
            if (acknowledged.count(alarm_id) == 0) {
                filtered.push_back(alarm);
            }
        }
        alarm_list = std::move(filtered);
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"alarms", alarm_list}});
}

std::string TcpCommandDispatcher::HandleAlarmsClear(const std::string& id, const nlohmann::json& /*params*/) {
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged_alarm_ids_.clear();
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"ok", true}});
}

std::string TcpCommandDispatcher::HandleAlarmsAcknowledge(const std::string& id, const nlohmann::json& params) {
    std::string alarm_id = params.value("id", "");
    if (alarm_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3201, "Missing 'id' parameter");
    }
    {
        std::lock_guard<std::mutex> lock(alarms_mutex_);
        acknowledged_alarm_ids_.insert(alarm_id);
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"ok", true}});
}

std::string TcpCommandDispatcher::HandleRecipeList(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3300, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::ListRecipesRequest request;
    request.status = params.value("status", "");
    request.query = params.value("query", "");
    request.tag = params.value("tag", "");

    auto result = recipeFacade_->ListRecipes(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3301, result.GetError().GetMessage());
    }

    nlohmann::json recipes = nlohmann::json::array();
    for (const auto& recipe : result.Value().recipes) {
        recipes.push_back(RecipeJsonSerializer::RecipeToJson(recipe));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipes", recipes}});
}

std::string TcpCommandDispatcher::HandleRecipeGet(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3310, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3311, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::GetRecipeRequest request;
    request.recipe_id = recipe_id;

    auto result = recipeFacade_->GetRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3312, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3320, "TcpRecipeFacade not available");
    }

    std::string name = params.value("name", "");
    if (name.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3321, "Missing 'name' parameter");
    }

    Application::UseCases::Recipes::CreateRecipeRequest request;
    request.name = name;
    request.description = params.value("description", "");
    request.tags = ParseTags(params);
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->CreateRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3322, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeUpdate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3330, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3331, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::UpdateRecipeRequest request;
    request.recipe_id = recipe_id;
    if (params.contains("name")) {
        request.name = params.value("name", "");
        request.update_name = true;
    }
    if (params.contains("description")) {
        request.description = params.value("description", "");
        request.update_description = true;
    }
    if (params.contains("tags")) {
        request.tags = ParseTags(params);
        request.update_tags = true;
    }
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->UpdateRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3332, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"recipe", RecipeJsonSerializer::RecipeToJson(result.Value().recipe)}});
}

std::string TcpCommandDispatcher::HandleRecipeArchive(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3340, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3341, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ArchiveRecipeRequest request;
    request.recipe_id = recipe_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->ArchiveRecipe(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3342, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"archived", result.Value().archived}});
}

std::string TcpCommandDispatcher::HandleRecipeDraftCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3350, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string template_id = params.value("templateId", "");
    if (template_id.empty()) {
        template_id = params.value("template_id", "");
    }
    if (recipe_id.empty() || template_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3351, "Missing 'recipeId' or 'templateId' parameter");
    }

    Application::UseCases::Recipes::CreateDraftVersionRequest request;
    request.recipe_id = recipe_id;
    request.template_id = template_id;
    request.base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    request.version_label = params.value("versionLabel", params.value("version_label", ""));
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.created_by = params.value("actor", "");

    auto result = recipeFacade_->CreateDraft(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3352, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeDraftUpdate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3360, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3361, "Missing 'recipeId' or 'versionId' parameter");
    }

    std::vector<ParameterValueEntry> parameters;
    std::string parse_error;
    if (!ParseParameterEntries(params, parameters, parse_error)) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3362, parse_error);
    }

    Application::UseCases::Recipes::UpdateDraftVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.parameters = parameters;
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.editor = params.value("actor", "");

    auto result = recipeFacade_->UpdateDraft(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3363, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipePublish(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3370, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3371, "Missing 'recipeId' or 'versionId' parameter");
    }

    Application::UseCases::Recipes::PublishRecipeVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->PublishRecipeVersion(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3372, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeVersions(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3380, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3381, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ListRecipeVersionsRequest request;
    request.recipe_id = recipe_id;

    auto result = recipeFacade_->ListRecipeVersions(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3382, result.GetError().GetMessage());
    }

    nlohmann::json versions = nlohmann::json::array();
    for (const auto& version : result.Value().versions) {
        versions.push_back(RecipeJsonSerializer::RecipeVersionToJson(version));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"versions", versions}});
}

std::string TcpCommandDispatcher::HandleRecipeVersionCreate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3390, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3391, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::CreateVersionFromPublishedRequest request;
    request.recipe_id = recipe_id;
    request.base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    request.version_label = params.value("versionLabel", params.value("version_label", ""));
    request.change_note = params.value("changeNote", params.value("change_note", ""));
    request.created_by = params.value("actor", "");

    auto result = recipeFacade_->CreateVersionFromPublished(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3392, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"version", RecipeJsonSerializer::RecipeVersionToJson(result.Value().version)}});
}

std::string TcpCommandDispatcher::HandleRecipeCompare(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3400, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string base_version_id = params.value("baseVersionId", params.value("base_version_id", ""));
    std::string version_id = params.value("versionId", params.value("version_id", ""));
    if (recipe_id.empty() || base_version_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3401, "Missing 'recipeId', 'baseVersionId', or 'versionId' parameter");
    }

    Application::UseCases::Recipes::CompareRecipeVersionsRequest request;
    request.recipe_id = recipe_id;
    request.from_version_id = base_version_id;
    request.to_version_id = version_id;

    auto result = recipeFacade_->CompareRecipeVersions(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3402, result.GetError().GetMessage());
    }

    nlohmann::json changes = nlohmann::json::array();
    for (const auto& change : result.Value().changes) {
        changes.push_back(RecipeJsonSerializer::FieldChangeToJson(change));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"changes", changes}});
}

std::string TcpCommandDispatcher::HandleRecipeActivate(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3410, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    std::string version_id = params.value("versionId", "");
    if (version_id.empty()) {
        version_id = params.value("version_id", "");
    }
    if (recipe_id.empty() || version_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3411, "Missing 'recipeId' or 'versionId' parameter");
    }

    Application::UseCases::Recipes::ActivateRecipeVersionRequest request;
    request.recipe_id = recipe_id;
    request.version_id = version_id;
    request.actor = params.value("actor", "");

    auto result = recipeFacade_->ActivateRecipeVersion(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3412, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"activated", true}});
}

std::string TcpCommandDispatcher::HandleRecipeTemplates(const std::string& id, const nlohmann::json& /*params*/) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3420, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::ListTemplatesRequest request;
    auto result = recipeFacade_->ListTemplates(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3421, result.GetError().GetMessage());
    }

    nlohmann::json templates = nlohmann::json::array();
    for (const auto& tmpl : result.Value().templates) {
        templates.push_back(RecipeJsonSerializer::TemplateToJson(tmpl));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"templates", templates}});
}

std::string TcpCommandDispatcher::HandleRecipeSchemaDefault(const std::string& id, const nlohmann::json& /*params*/) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3430, "TcpRecipeFacade not available");
    }

    Application::UseCases::Recipes::GetDefaultParameterSchemaRequest request;
    auto result = recipeFacade_->GetDefaultParameterSchema(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3431, result.GetError().GetMessage());
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"schema", RecipeJsonSerializer::ParameterSchemaToJson(result.Value().schema)}});
}

std::string TcpCommandDispatcher::HandleRecipeAudit(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3440, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3441, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::GetRecipeAuditRequest request;
    request.recipe_id = recipe_id;
    std::string version_id = params.value("versionId", params.value("version_id", ""));
    if (!version_id.empty()) {
        request.version_id = version_id;
    }

    auto result = recipeFacade_->GetRecipeAudit(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3442, result.GetError().GetMessage());
    }

    nlohmann::json records = nlohmann::json::array();
    for (const auto& record : result.Value().records) {
        records.push_back(RecipeJsonSerializer::AuditRecordToJson(record));
    }
    return GatewayJsonProtocol::MakeSuccessResponse(id, {{"records", records}});
}

std::string TcpCommandDispatcher::HandleRecipeExport(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3450, "TcpRecipeFacade not available");
    }

    std::string recipe_id = params.value("recipeId", "");
    if (recipe_id.empty()) {
        recipe_id = params.value("recipe_id", "");
    }
    if (recipe_id.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3451, "Missing 'recipeId' parameter");
    }

    Application::UseCases::Recipes::ExportRecipeBundlePayloadRequest request;
    request.recipe_id = recipe_id;
    const std::string output_path = params.value("outputPath", params.value("output_path", ""));

    auto result = recipeFacade_->ExportBundle(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3452, result.GetError().GetMessage());
    }

    if (!output_path.empty()) {
        std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3453, "Failed to write export file");
        }
        out << result.Value().bundle_json;
        if (!out) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3454, "Failed to write export file");
        }
    }

    nlohmann::json response = {
        {"outputPath", output_path},
        {"recipeCount", result.Value().recipe_count},
        {"versionCount", result.Value().version_count},
        {"auditCount", result.Value().audit_count}
    };
    if (output_path.empty()) {
        response["bundleJson"] = result.Value().bundle_json;
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, response);
}

std::string TcpCommandDispatcher::HandleMotionCoordStatus(const std::string& id, const nlohmann::json& params) {
    if (!motionFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2200, "TcpMotionFacade not available");
    }

    const auto coord_sys = static_cast<int16_t>(params.value("coord_sys", 1));
    auto coord_result = motionFacade_->GetCoordinateSystemStatus(coord_sys);
    if (coord_result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 2201, coord_result.GetError().GetMessage());
    }

    auto position_result = motionFacade_->GetCurrentPosition();
    auto x_status_result = motionFacade_->GetAxisMotionStatus(LogicalAxisId::X);
    auto y_status_result = motionFacade_->GetAxisMotionStatus(LogicalAxisId::Y);
    auto buffer_space_result = motionFacade_->GetInterpolationBufferSpace(coord_sys);
    auto lookahead_space_result = motionFacade_->GetLookAheadBufferSpace(coord_sys);

    const auto& coord_status = coord_result.Value();
    nlohmann::json response{
        {"coord_sys", coord_sys},
        {"state", static_cast<int>(coord_status.state)},
        {"is_moving", coord_status.is_moving},
        {"remaining_segments", coord_status.remaining_segments},
        {"current_velocity", coord_status.current_velocity},
        {"raw_status_word", coord_status.raw_status_word},
        {"raw_segment", coord_status.raw_segment},
        {"mc_status_ret", coord_status.mc_status_ret},
    };

    if (buffer_space_result.IsSuccess()) {
        response["buffer_space"] = buffer_space_result.Value();
    }
    if (lookahead_space_result.IsSuccess()) {
        response["lookahead_space"] = lookahead_space_result.Value();
    }
    if (position_result.IsSuccess()) {
        response["position"] = {
            {"x", position_result.Value().x},
            {"y", position_result.Value().y},
        };
    }
    if (x_status_result.IsSuccess()) {
        response["axes"]["X"] = {
            {"position", x_status_result.Value().axis_position_mm},
            {"velocity", x_status_result.Value().velocity},
            {"state", static_cast<int>(x_status_result.Value().state)},
            {"enabled", x_status_result.Value().enabled},
            {"homed", x_status_result.Value().homing_state == "homed"},
            {"in_position", x_status_result.Value().in_position},
            {"has_error", x_status_result.Value().has_error},
            {"error_code", x_status_result.Value().error_code},
            {"servo_alarm", x_status_result.Value().servo_alarm},
            {"following_error", x_status_result.Value().following_error},
            {"home_failed", x_status_result.Value().home_failed},
            {"hard_limit_positive", x_status_result.Value().hard_limit_positive},
            {"hard_limit_negative", x_status_result.Value().hard_limit_negative},
            {"soft_limit_positive", x_status_result.Value().soft_limit_positive},
            {"soft_limit_negative", x_status_result.Value().soft_limit_negative},
            {"home_signal", x_status_result.Value().home_signal},
            {"index_signal", x_status_result.Value().index_signal},
            {"selected_feedback_source", x_status_result.Value().selected_feedback_source},
            {"prf_position_mm", x_status_result.Value().profile_position_mm},
            {"enc_position_mm", x_status_result.Value().encoder_position_mm},
            {"prf_velocity_mm_s", x_status_result.Value().profile_velocity_mm_s},
            {"enc_velocity_mm_s", x_status_result.Value().encoder_velocity_mm_s},
            {"prf_position_ret", x_status_result.Value().profile_position_ret},
            {"enc_position_ret", x_status_result.Value().encoder_position_ret},
            {"prf_velocity_ret", x_status_result.Value().profile_velocity_ret},
            {"enc_velocity_ret", x_status_result.Value().encoder_velocity_ret},
        };
    }
    if (y_status_result.IsSuccess()) {
        response["axes"]["Y"] = {
            {"position", y_status_result.Value().axis_position_mm},
            {"velocity", y_status_result.Value().velocity},
            {"state", static_cast<int>(y_status_result.Value().state)},
            {"enabled", y_status_result.Value().enabled},
            {"homed", y_status_result.Value().homing_state == "homed"},
            {"in_position", y_status_result.Value().in_position},
            {"has_error", y_status_result.Value().has_error},
            {"error_code", y_status_result.Value().error_code},
            {"servo_alarm", y_status_result.Value().servo_alarm},
            {"following_error", y_status_result.Value().following_error},
            {"home_failed", y_status_result.Value().home_failed},
            {"hard_limit_positive", y_status_result.Value().hard_limit_positive},
            {"hard_limit_negative", y_status_result.Value().hard_limit_negative},
            {"soft_limit_positive", y_status_result.Value().soft_limit_positive},
            {"soft_limit_negative", y_status_result.Value().soft_limit_negative},
            {"home_signal", y_status_result.Value().home_signal},
            {"index_signal", y_status_result.Value().index_signal},
            {"selected_feedback_source", y_status_result.Value().selected_feedback_source},
            {"prf_position_mm", y_status_result.Value().profile_position_mm},
            {"enc_position_mm", y_status_result.Value().encoder_position_mm},
            {"prf_velocity_mm_s", y_status_result.Value().profile_velocity_mm_s},
            {"enc_velocity_mm_s", y_status_result.Value().encoder_velocity_mm_s},
            {"prf_position_ret", y_status_result.Value().profile_position_ret},
            {"enc_position_ret", y_status_result.Value().encoder_position_ret},
            {"prf_velocity_ret", y_status_result.Value().profile_velocity_ret},
            {"enc_velocity_ret", y_status_result.Value().encoder_velocity_ret},
        };
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, response);
}

std::string TcpCommandDispatcher::HandleRecipeImport(const std::string& id, const nlohmann::json& params) {
    if (!recipeFacade_) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3460, "TcpRecipeFacade not available");
    }

    std::string bundle_json = params.value("bundleJson", params.value("bundle_json", ""));
    std::string bundle_path = params.value("bundlePath", params.value("bundle_path", ""));
    if (bundle_json.empty() && bundle_path.empty()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3461, "Missing bundle payload");
    }

    if (bundle_json.empty() && !bundle_path.empty()) {
        std::ifstream in(bundle_path, std::ios::binary);
        if (!in) {
            return GatewayJsonProtocol::MakeErrorResponse(id, 3463, "Bundle file not found");
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        bundle_json = buffer.str();
    }

    Application::UseCases::Recipes::ImportRecipeBundlePayloadRequest request;
    request.bundle_json = bundle_json;
    request.resolution_strategy = params.value("resolution", "");
    request.dry_run = params.value("dryRun", params.value("dry_run", false));
    request.actor = params.value("actor", "");
    request.source_label = bundle_path;

    auto result = recipeFacade_->ImportBundle(request);
    if (result.IsError()) {
        return GatewayJsonProtocol::MakeErrorResponse(id, 3462, result.GetError().GetMessage());
    }

    nlohmann::json conflicts = nlohmann::json::array();
    for (const auto& conflict : result.Value().conflicts) {
        conflicts.push_back(ImportConflictToJson(conflict));
    }

    return GatewayJsonProtocol::MakeSuccessResponse(id, {
        {"status", result.Value().status},
        {"importedCount", result.Value().imported_count},
        {"conflicts", conflicts}
    });
}

std::string TcpCommandDispatcher::ResolveActiveDxfJobState() const {
    if (!dispensingFacade_) {
        return "";
    }

    std::string job_id;
    {
        std::lock_guard<std::mutex> lock(dxf_mutex_);
        job_id = active_dxf_job_id_;
    }
    if (job_id.empty()) {
        return "";
    }

    auto status_result = dispensingFacade_->GetDxfJobStatus(job_id);
    if (status_result.IsError()) {
        return "";
    }
    return status_result.Value().state;
}

} // namespace Siligen::Adapters::Tcp






