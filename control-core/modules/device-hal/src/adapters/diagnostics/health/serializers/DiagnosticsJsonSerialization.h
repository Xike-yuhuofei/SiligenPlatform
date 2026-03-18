// DiagnosticsJsonSerialization.h - JSON 工具函数
// 注意：工艺/测试结果的结构化定义与序列化已由 Domain 层统一负责。
// 若涉及 TestRecord/TestDataTypes，请使用 domain/diagnostics/domain-services/ProcessResultSerialization。
#pragma once

#include "shared/types/Result.h"
#include "shared/types/Error.h"
#include "shared/types/SerializationTypes.h"
#include "domain/diagnostics/value-objects/DiagnosticTypes.h"
#include "domain/dispensing/value-objects/DispensingPlan.h"
#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/value-objects/TrajectoryAnalysisTypes.h"
#include "domain/motion/value-objects/TrajectoryTypes.h"
#include "modules/device-hal/src/adapters/diagnostics/health/serializers/EnumConverters.h"
#include "nlohmann/json.hpp"
#include <string>
#include <vector>

namespace Siligen {
namespace Infrastructure {
namespace Serialization {

using json = nlohmann::json;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::CreateSerializationError;
using Siligen::Shared::Types::CreateJsonParseError;
using Siligen::Shared::Types::CreateMissingFieldError;

// ========================================
// JSON 解析和序列化
// ========================================

/**
 * @brief 解析 JSON 字符串
 * @param jsonStr JSON 字符串
 * @return Result<json> 解析后的 JSON 对象
 */
inline Result<json> Parse(const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return Result<json>::Success(j);
    } catch (const json::parse_error& e) {
        return Result<json>::Failure(
            CreateJsonParseError(std::string("JSON parse error: ") + e.what())
        );
    }
}

/**
 * @brief 将 JSON 对象转字符串
 * @param j JSON 对象
 * @return Result<std::string> JSON 字符串
 */
inline Result<std::string> Dump(const json& j) {
    try {
        return Result<std::string>::Success(j.dump(4));  // 4 空格缩进
    } catch (const json::type_error& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::SERIALIZATION_FAILED,
                                     std::string("JSON dump error: ") + e.what())
        );
    }
}

// ========================================
// 字段获取辅助函数
// ========================================

/**
 * @brief 获取整数字段
 */
inline Result<int> GetIntField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<int>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<int>::Success(j[fieldName].get<int>());
    } catch (const json::type_error& e) {
        return Result<int>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not an integer")
        );
    }
}

/**
 * @brief 获取浮点数字段
 */
inline Result<double> GetDoubleField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<double>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<double>::Success(j[fieldName].get<double>());
    } catch (const json::type_error& e) {
        return Result<double>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a number")
        );
    }
}

/**
 * @brief 获取字符串字段
 */
inline Result<std::string> GetStringField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<std::string>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<std::string>::Success(j[fieldName].get<std::string>());
    } catch (const json::type_error& e) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a string")
        );
    }
}

/**
 * @brief 获取布尔字段
 */
inline Result<bool> GetBoolField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<bool>::Failure(CreateMissingFieldError(fieldName));
    }

    try {
        return Result<bool>::Success(j[fieldName].get<bool>());
    } catch (const json::type_error& e) {
        return Result<bool>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not a boolean")
        );
    }
}

/**
 * @brief 获取数组字段
 */
template<typename T>
inline Result<std::vector<T>> GetArrayField(const json& j, const std::string& fieldName) {
    if (!j.contains(fieldName)) {
        return Result<std::vector<T>>::Failure(CreateMissingFieldError(fieldName));
    }

    if (!j[fieldName].is_array()) {
        return Result<std::vector<T>>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " is not an array")
        );
    }

    try {
        return Result<std::vector<T>>::Success(j[fieldName].get<std::vector<T>>());
    } catch (const json::type_error& e) {
        return Result<std::vector<T>>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + fieldName + " array parsing failed")
        );
    }
}

}  // namespace Serialization
}  // namespace Infrastructure

// ========================================
// 基础类型 JSON 序列化支持（在 Siligen 命名空间中）
// ========================================

using json = nlohmann::json;

// Point2D 序列化辅助函数
inline json Point2DToJson(const Shared::Types::Point2D& p) {
    return json{{"x", p.x}, {"y", p.y}};
}

inline Shared::Types::Point2D Point2DFromJson(const json& j) {
    Shared::Types::Point2D p;
    p.x = j.at("x").get<double>();
    p.y = j.at("y").get<double>();
    return p;
}

// Point3D 序列化辅助函数
inline json Point3DToJson(const Siligen::Point3D& p) {
    return json{{"x", p.x}, {"y", p.y}, {"z", p.z}};
}

inline Siligen::Point3D Point3DFromJson(const json& j) {
    Siligen::Point3D p;
    p.x = j.at("x").get<double>();
    p.y = j.at("y").get<double>();
    p.z = j.value("z", 0.0);
    return p;
}

// std::vector<Point2D> 序列化辅助函数
inline std::vector<json> Point2DVectorToJsonArray(const std::vector<Shared::Types::Point2D>& points) {
    std::vector<json> arr;
    arr.reserve(points.size());
    for (const auto& p : points) {
        arr.push_back(Point2DToJson(p));
    }
    return arr;
}

inline std::vector<Shared::Types::Point2D> Point2DVectorFromJsonArray(const json& j) {
    std::vector<Shared::Types::Point2D> points;
    if (!j.is_array()) {
        return points;
    }
    points.reserve(j.size());
    for (const auto& item : j) {
        points.push_back(Point2DFromJson(item));
    }
    return points;
}

// ========================================
// MotionSegment JSON 支持
// ========================================

inline void to_json(json& j, const MotionSegment& segment) {
    j = json{
        {"startPoint", Point2DToJson(segment.start_point)},
        {"endPoint", Point2DToJson(segment.end_point)},
        {"startVelocity", segment.start_velocity},
        {"endVelocity", segment.end_velocity},
        {"maxVelocity", segment.max_velocity},
        {"maxAcceleration", segment.max_acceleration},
        {"length", segment.length},
        {"dispensingStartOffset", segment.dispensing_start_offset},
        {"dispensingEndOffset", segment.dispensing_end_offset},
        {"segmentType", static_cast<int>(segment.segment_type)},
        {"curvatureRadius", segment.curvature_radius},
        {"enableDispensing", segment.enable_dispensing}
    };
}

inline void from_json(const json& j, MotionSegment& segment) {
    if (j.contains("startPoint")) {
        segment.start_point = Point2DFromJson(j.at("startPoint"));
    }
    if (j.contains("endPoint")) {
        segment.end_point = Point2DFromJson(j.at("endPoint"));
    }

    segment.start_velocity = static_cast<Siligen::Shared::Types::float32>(j.value("startVelocity", 0.0));
    segment.end_velocity = static_cast<Siligen::Shared::Types::float32>(j.value("endVelocity", 0.0));
    segment.max_velocity = static_cast<Siligen::Shared::Types::float32>(j.value("maxVelocity", 0.0));
    segment.max_acceleration = static_cast<Siligen::Shared::Types::float32>(j.value("maxAcceleration", 0.0));
    segment.length = static_cast<Siligen::Shared::Types::float32>(j.value("length", 0.0));
    segment.dispensing_start_offset =
        static_cast<Siligen::Shared::Types::float32>(j.value("dispensingStartOffset", 0.0));
    segment.dispensing_end_offset =
        static_cast<Siligen::Shared::Types::float32>(j.value("dispensingEndOffset", 0.0));
    segment.segment_type = static_cast<SegmentType>(j.value("segmentType", 0));
    segment.curvature_radius = static_cast<Siligen::Shared::Types::float32>(j.value("curvatureRadius", 0.0));
    segment.enable_dispensing = j.value("enableDispensing", false);
}

// ========================================
// PlanningReport JSON 支持
// ========================================

namespace Domain::Trajectory::ValueObjects {
using json = nlohmann::json;

inline void to_json(json& j, const PlanningReport& r) {
    j = json{
        {"totalLengthMm", r.total_length_mm},
        {"totalTimeS", r.total_time_s},
        {"maxVelocityObserved", r.max_velocity_observed},
        {"maxAccelerationObserved", r.max_acceleration_observed},
        {"maxJerkObserved", r.max_jerk_observed},
        {"constraintViolations", r.constraint_violations},
        {"timeIntegrationErrorS", r.time_integration_error_s},
        {"timeIntegrationFallbacks", r.time_integration_fallbacks},
        {"jerkLimitEnforced", r.jerk_limit_enforced},
        {"jerkPlanFailed", r.jerk_plan_failed},
        {"segmentCount", r.segment_count},
        {"rapidSegmentCount", r.rapid_segment_count},
        {"rapidLengthMm", r.rapid_length_mm},
        {"cornerSegmentCount", r.corner_segment_count},
        {"discontinuityCount", r.discontinuity_count}
    };
}

inline void from_json(const json& j, PlanningReport& r) {
    r.total_length_mm = static_cast<Siligen::Shared::Types::float32>(j.value("totalLengthMm", 0.0));
    r.total_time_s = static_cast<Siligen::Shared::Types::float32>(j.value("totalTimeS", 0.0));
    r.max_velocity_observed = static_cast<Siligen::Shared::Types::float32>(j.value("maxVelocityObserved", 0.0));
    r.max_acceleration_observed =
        static_cast<Siligen::Shared::Types::float32>(j.value("maxAccelerationObserved", 0.0));
    r.max_jerk_observed = static_cast<Siligen::Shared::Types::float32>(j.value("maxJerkObserved", 0.0));
    r.constraint_violations = j.value("constraintViolations", 0);
    r.time_integration_error_s =
        static_cast<Siligen::Shared::Types::float32>(j.value("timeIntegrationErrorS", 0.0));
    r.time_integration_fallbacks = j.value("timeIntegrationFallbacks", 0);
    r.jerk_limit_enforced = j.value("jerkLimitEnforced", false);
    r.jerk_plan_failed = j.value("jerkPlanFailed", false);
    r.segment_count = j.value("segmentCount", 0);
    r.rapid_segment_count = j.value("rapidSegmentCount", 0);
    r.rapid_length_mm = static_cast<Siligen::Shared::Types::float32>(j.value("rapidLengthMm", 0.0));
    r.corner_segment_count = j.value("cornerSegmentCount", 0);
    r.discontinuity_count = j.value("discontinuityCount", 0);
}

}  // namespace Domain::Trajectory::ValueObjects

// ========================================
// HomingTestData 辅助类型 JSON 支持
// ========================================

// 注意：LimitSwitchState 和 HomingResult 的 to_json/from_json 函数
// 需要在这些类型所在的命名空间（Siligen）中定义，以便 ADL 能够找到它们

}  // namespace Siligen


// 为了让 ADL 工作，在 Siligen::Domain::Motion::ValueObjects 命名空间中定义这些函数
namespace Siligen::Domain::Motion::ValueObjects {

using json = nlohmann::json;

/**
 * @brief MotionTrajectoryPoint 转 JSON
 */
inline void to_json(json& j, const MotionTrajectoryPoint& p) {
    j = json{
        {"t", p.t},
        {"position", Point3DToJson(p.position)},
        {"velocity", Point3DToJson(p.velocity)},
        {"processTag", static_cast<int>(p.process_tag)},
        {"dispenseOn", p.dispense_on},
        {"flowRate", p.flow_rate}
    };
}

/**
 * @brief JSON 转 MotionTrajectoryPoint
 */
inline void from_json(const json& j, MotionTrajectoryPoint& p) {
    p.t = static_cast<Siligen::Shared::Types::float32>(j.value("t", 0.0));
    if (j.contains("position")) {
        p.position = Point3DFromJson(j.at("position"));
    }
    if (j.contains("velocity")) {
        p.velocity = Point3DFromJson(j.at("velocity"));
    }
    p.process_tag = static_cast<ProcessTag>(j.value("processTag", 0));
    p.dispense_on = j.value("dispenseOn", false);
    p.flow_rate = static_cast<Siligen::Shared::Types::float32>(j.value("flowRate", 0.0));
}

/**
 * @brief MotionTrajectory 转 JSON
 */
inline void to_json(json& j, const MotionTrajectory& t) {
    j = json{
        {"points", t.points},
        {"totalTime", t.total_time},
        {"totalLength", t.total_length},
        {"planningReport", t.planning_report}
    };
}

/**
 * @brief JSON 转 MotionTrajectory
 */
inline void from_json(const json& j, MotionTrajectory& t) {
    if (j.contains("points")) {
        t.points = j.at("points").get<std::vector<MotionTrajectoryPoint>>();
    }
    t.total_time = static_cast<Siligen::Shared::Types::float32>(j.value("totalTime", 0.0));
    t.total_length = static_cast<Siligen::Shared::Types::float32>(j.value("totalLength", 0.0));
    if (j.contains("planningReport")) {
        t.planning_report =
            j.at("planningReport").get<Siligen::Domain::Trajectory::ValueObjects::PlanningReport>();
    }
}

/**
 * @brief LimitSwitchState 转 JSON
 */
inline void to_json(json& j, const LimitSwitchState& state) {
    j = json{
        {"positiveLimitTriggered", state.positiveLimitTriggered},
        {"negativeLimitTriggered", state.negativeLimitTriggered},
        {"timestamp", state.timestamp}
    };
}

/**
 * @brief JSON 转 LimitSwitchState
 */
inline void from_json(const json& j, LimitSwitchState& state) {
    state.positiveLimitTriggered = j.at("positiveLimitTriggered").get<bool>();
    state.negativeLimitTriggered = j.at("negativeLimitTriggered").get<bool>();
    state.timestamp = j.at("timestamp").get<std::int64_t>();
}

/**
 * @brief HomingResult 转 JSON
 */
inline void to_json(json& j, const HomingResult& result) {
    j = json{
        {"success", result.success},
        {"durationMs", result.durationMs},
        {"finalPosition", result.finalPosition}
    };

    // 可选字段：failureReason
    if (result.failureReason.has_value()) {
        j["failureReason"] = result.failureReason.value();
    } else {
        j["failureReason"] = nullptr;
    }
}

/**
 * @brief JSON 转 HomingResult
 */
inline void from_json(const json& j, HomingResult& result) {
    result.success = j.at("success").get<bool>();
    result.durationMs = j.at("durationMs").get<std::int32_t>();
    result.finalPosition = j.at("finalPosition").get<double>();

    // 可选字段：failureReason
    if (j.at("failureReason").is_null()) {
        result.failureReason = std::nullopt;
    } else {
        result.failureReason = j.at("failureReason").get<std::string>();
    }
}

// ========================================
// InterpolationTestData 辅助类型 JSON 支持
// ========================================

/**
 * @brief InterpolationParameters 转 JSON
 */
inline void to_json(json& j, const InterpolationParameters& params) {
    j = json{
        {"targetFeedRate", params.targetFeedRate},
        {"acceleration", params.acceleration},
        {"interpolationCycle", params.interpolationCycle}
    };
}

/**
 * @brief JSON 转 InterpolationParameters
 */
inline void from_json(const json& j, InterpolationParameters& params) {
    params.targetFeedRate = j.at("targetFeedRate").get<double>();
    params.acceleration = j.at("acceleration").get<double>();
    params.interpolationCycle = j.at("interpolationCycle").get<int>();
}

/**
 * @brief PathQualityMetrics 转 JSON
 */
inline void to_json(json& j, const PathQualityMetrics& metrics) {
    j = json{
        {"maxPathError", metrics.maxPathError},
        {"avgPathError", metrics.avgPathError},
        {"circularityError", metrics.circularityError},
        {"radiusError", metrics.radiusError}
    };
}

/**
 * @brief JSON 转 PathQualityMetrics
 */
inline void from_json(const json& j, PathQualityMetrics& metrics) {
    metrics.maxPathError = j.at("maxPathError").get<double>();
    metrics.avgPathError = j.at("avgPathError").get<double>();
    metrics.circularityError = j.at("circularityError").get<double>();
    metrics.radiusError = j.at("radiusError").get<double>();
}

/**
 * @brief MotionQualityMetrics 转 JSON
 */
inline void to_json(json& j, const MotionQualityMetrics& metrics) {
    j = json{
        {"maxVelocity", metrics.maxVelocity},
        {"avgVelocity", metrics.avgVelocity},
        {"maxAcceleration", metrics.maxAcceleration},
        {"accelerationDiscontinuities", metrics.accelerationDiscontinuities},
        {"velocitySmoothness", metrics.velocitySmoothness}
    };
}

/**
 * @brief JSON 转 MotionQualityMetrics
 */
inline void from_json(const json& j, MotionQualityMetrics& metrics) {
    metrics.maxVelocity = j.at("maxVelocity").get<double>();
    metrics.avgVelocity = j.at("avgVelocity").get<double>();
    metrics.maxAcceleration = j.at("maxAcceleration").get<double>();
    metrics.accelerationDiscontinuities = j.at("accelerationDiscontinuities").get<int>();
    metrics.velocitySmoothness = j.at("velocitySmoothness").get<double>();
}

// ========================================
// TriggerPoint JSON 支持
// ========================================

/**
 * @brief TriggerPoint 转 JSON
 */
inline void to_json(json& j, const TriggerPoint& p) {
    j = json{
        {"axis", p.axis},
        {"position", p.position},
        {"outputPort", p.outputPort},
        {"action", Siligen::Infrastructure::Serialization::ToString(p.action)}
    };
}

/**
 * @brief JSON 转 TriggerPoint
 */
inline void from_json(const json& j, TriggerPoint& p) {
    p.axis = static_cast<Siligen::Shared::Types::LogicalAxisId>(j.at("axis").get<int>());
    p.position = j.at("position").get<double>();
    p.outputPort = j.at("outputPort").get<int>();

    auto actionResult = Siligen::Infrastructure::Serialization::FromString_TriggerAction(j.at("action").get<std::string>());
    if (actionResult.IsSuccess()) {
        p.action = actionResult.Value();
    } else {
        // 设置默认值
        p.action = TriggerAction::TurnOn;
    }
}

// ========================================
// TriggerEvent JSON 支持
// ========================================

/**
 * @brief TriggerEvent 转 JSON
 */
inline void to_json(json& j, const TriggerEvent& e) {
    j = json{
        {"timestamp", e.timestamp},
        {"triggerPointId", e.triggerPointId},
        {"actualPosition", e.actualPosition},
        {"positionError", e.positionError},
        {"responseTimeUs", e.responseTimeUs},
        {"outputVerified", e.outputVerified}
    };
}

/**
 * @brief JSON 转 TriggerEvent
 */
inline void from_json(const json& j, TriggerEvent& e) {
    e.timestamp = j.at("timestamp").get<std::int64_t>();
    e.triggerPointId = j.at("triggerPointId").get<int>();
    e.actualPosition = j.at("actualPosition").get<double>();
    e.positionError = j.at("positionError").get<double>();
    e.responseTimeUs = j.at("responseTimeUs").get<std::int32_t>();
    e.outputVerified = j.at("outputVerified").get<bool>();
}

// ========================================
// CMP 相关类型 JSON 支持
// ========================================

/**
 * @brief CMPParameters 转 JSON
 */
inline void to_json(json& j, const CMPParameters& p) {
    j = json{
        {"compensationGain", p.compensationGain},
        {"lookaheadPoints", p.lookaheadPoints},
        {"maxCompensation", p.maxCompensation}
    };
}

/**
 * @brief JSON 转 CMPParameters
 */
inline void from_json(const json& j, CMPParameters& p) {
    p.compensationGain = j.at("compensationGain").get<double>();
    p.lookaheadPoints = j.at("lookaheadPoints").get<int>();
    p.maxCompensation = j.at("maxCompensation").get<double>();
}

}  // namespace Siligen::Domain::Motion::ValueObjects

namespace Siligen::Domain::Dispensing::ValueObjects {
using json = nlohmann::json;

/**
 * @brief SafetyBoundary 转 JSON
 */
inline void to_json(json& j, const SafetyBoundary& s) {
    j = json{
        {"durationMs", s.duration_ms},
        {"valveResponseMs", s.valve_response_ms},
        {"marginMs", s.margin_ms},
        {"minIntervalMs", s.min_interval_ms},
        {"downgradeOnViolation", s.downgrade_on_violation}
    };
}

/**
 * @brief JSON 转 SafetyBoundary
 */
inline void from_json(const json& j, SafetyBoundary& s) {
    s.duration_ms = j.value("durationMs", 0);
    s.valve_response_ms = j.value("valveResponseMs", 0);
    s.margin_ms = j.value("marginMs", 0);
    s.min_interval_ms = j.value("minIntervalMs", 0);
    s.downgrade_on_violation = j.value("downgradeOnViolation", false);
}

/**
 * @brief QualityMetrics 转 JSON
 */
inline void to_json(json& j, const QualityMetrics& m) {
    j = json{
        {"lineWidthCv", m.line_width_cv},
        {"cornerDeviationPct", m.corner_deviation_pct},
        {"gluePileReductionPct", m.glue_pile_reduction_pct},
        {"triggerIntervalErrorPct", m.trigger_interval_error_pct},
        {"runCount", m.run_count},
        {"interruptionCount", m.interruption_count}
    };
}

/**
 * @brief JSON 转 QualityMetrics
 */
inline void from_json(const json& j, QualityMetrics& m) {
    m.line_width_cv = static_cast<Siligen::Shared::Types::float32>(j.value("lineWidthCv", 0.0));
    m.corner_deviation_pct = static_cast<Siligen::Shared::Types::float32>(j.value("cornerDeviationPct", 0.0));
    m.glue_pile_reduction_pct = static_cast<Siligen::Shared::Types::float32>(j.value("gluePileReductionPct", 0.0));
    m.trigger_interval_error_pct =
        static_cast<Siligen::Shared::Types::float32>(j.value("triggerIntervalErrorPct", 0.0));
    m.run_count = j.value("runCount", 0);
    m.interruption_count = j.value("interruptionCount", 0);
}

/**
 * @brief DispenseCompensationProfile 转 JSON
 */
inline void to_json(json& j, const DispenseCompensationProfile& p) {
    j = json{
        {"openCompMs", p.open_comp_ms},
        {"closeCompMs", p.close_comp_ms},
        {"retractEnabled", p.retract_enabled},
        {"cornerPulseScale", p.corner_pulse_scale},
        {"curvatureSpeedFactor", p.curvature_speed_factor}
    };
}

/**
 * @brief JSON 转 DispenseCompensationProfile
 */
inline void from_json(const json& j, DispenseCompensationProfile& p) {
    p.open_comp_ms = static_cast<Siligen::Shared::Types::float32>(j.value("openCompMs", 0.0));
    p.close_comp_ms = static_cast<Siligen::Shared::Types::float32>(j.value("closeCompMs", 0.0));
    p.retract_enabled = j.value("retractEnabled", false);
    p.corner_pulse_scale = static_cast<Siligen::Shared::Types::float32>(j.value("cornerPulseScale", 1.0));
    p.curvature_speed_factor = static_cast<Siligen::Shared::Types::float32>(j.value("curvatureSpeedFactor", 0.8));
}

/**
 * @brief TriggerPlan 转 JSON
 */
inline void to_json(json& j, const TriggerPlan& plan) {
    j = json{
        {"strategy", Siligen::Infrastructure::Serialization::ToString(plan.strategy)},
        {"intervalMm", plan.interval_mm},
        {"subsegmentCount", plan.subsegment_count},
        {"dispenseOnlyCruise", plan.dispense_only_cruise},
        {"safety", plan.safety}
    };
}

/**
 * @brief JSON 转 TriggerPlan
 */
inline void from_json(const json& j, TriggerPlan& plan) {
    if (j.contains("strategy")) {
        auto strategy_result =
            Siligen::Infrastructure::Serialization::FromString_DispensingStrategy(j.at("strategy").get<std::string>());
        if (strategy_result.IsSuccess()) {
            plan.strategy = strategy_result.Value();
        }
    }

    plan.interval_mm = static_cast<Siligen::Shared::Types::float32>(j.value("intervalMm", 0.0));
    plan.subsegment_count = j.value("subsegmentCount", 0);
    plan.dispense_only_cruise = j.value("dispenseOnlyCruise", false);

    if (j.contains("safety")) {
        plan.safety = j.at("safety").get<SafetyBoundary>();
    }
}

/**
 * @brief DispensingPlan 转 JSON
 */
inline void to_json(json& j, const DispensingPlan& plan) {
    j = json{
        {"planId", plan.plan_id},
        {"name", plan.name},
        {"pathSource", plan.path_source},
        {"segments", plan.segments},
        {"triggerPlan", plan.trigger_plan},
        {"compensationProfile", plan.compensation_profile},
        {"status", Siligen::Infrastructure::Serialization::ToString(plan.status)},
        {"metrics", plan.metrics},
        {"estimatedLength", plan.EstimatedLength()}
    };
}

/**
 * @brief JSON 转 DispensingPlan
 */
inline void from_json(const json& j, DispensingPlan& plan) {
    plan.plan_id = j.value("planId", "");
    plan.name = j.value("name", "");
    plan.path_source = j.value("pathSource", "");

    if (j.contains("segments") && j.at("segments").is_array()) {
        plan.segments = j.at("segments").get<std::vector<MotionSegment>>();
    }

    if (j.contains("triggerPlan")) {
        plan.trigger_plan = j.at("triggerPlan").get<TriggerPlan>();
    }
    if (j.contains("compensationProfile")) {
        plan.compensation_profile = j.at("compensationProfile").get<DispenseCompensationProfile>();
    }

    if (j.contains("status")) {
        auto status_result =
            Siligen::Infrastructure::Serialization::FromString_PlanStatus(j.at("status").get<std::string>());
        if (status_result.IsSuccess()) {
            plan.status = status_result.Value();
        }
    }

    if (j.contains("metrics")) {
        plan.metrics = j.at("metrics").get<QualityMetrics>();
    }
}

}  // namespace Siligen::Domain::Dispensing::ValueObjects

namespace Siligen::Domain::Motion::ValueObjects {
using json = nlohmann::json;

/**
 * @brief TrajectoryAnalysis 转 JSON
 */
inline void to_json(json& j, const TrajectoryAnalysis& a) {
    j = json{
        {"maxDeviation",
         {{"positionDeviationMm", a.max_deviation.position_deviation_mm},
          {"timeDeviationMs", a.max_deviation.time_deviation_ms},
          {"velocityDeviationPercent", a.max_deviation.velocity_deviation_percent}}},
        {"avgDeviation",
         {{"positionDeviationMm", a.average_deviation.position_deviation_mm},
          {"timeDeviationMs", a.average_deviation.time_deviation_ms},
          {"velocityDeviationPercent", a.average_deviation.velocity_deviation_percent}}},
        {"rmsDeviation",
         {{"positionDeviationMm", a.rms_deviation.position_deviation_mm},
          {"timeDeviationMs", a.rms_deviation.time_deviation_ms},
          {"velocityDeviationPercent", a.rms_deviation.velocity_deviation_percent}}},
        {"triggerPrecision",
         {{"totalTriggerCount", a.trigger_precision.total_trigger_count},
          {"successTriggerCount", a.trigger_precision.success_trigger_count},
          {"averageTriggerDeviationMm", a.trigger_precision.average_trigger_deviation_mm},
          {"maxTriggerDeviationMm", a.trigger_precision.max_trigger_deviation_mm},
          {"triggerResponseTimeMs", a.trigger_precision.trigger_response_time_ms}}}
    };
}

/**
 * @brief JSON 转 TrajectoryAnalysis
 */
inline void from_json(const json& j, TrajectoryAnalysis& a) {
    auto parse_stats = [](const json& value, DeviationStatistics& out) {
        if (value.is_object()) {
            out.position_deviation_mm =
                static_cast<Siligen::Shared::Types::float32>(value.value("positionDeviationMm", 0.0));
            out.time_deviation_ms =
                static_cast<Siligen::Shared::Types::float32>(value.value("timeDeviationMs", 0.0));
            out.velocity_deviation_percent =
                static_cast<Siligen::Shared::Types::float32>(value.value("velocityDeviationPercent", 0.0));
            return;
        }

        if (value.is_number()) {
            out.position_deviation_mm = static_cast<Siligen::Shared::Types::float32>(value.get<double>());
            out.time_deviation_ms = 0.0f;
            out.velocity_deviation_percent = 0.0f;
        }
    };

    if (j.contains("maxDeviation")) {
        parse_stats(j.at("maxDeviation"), a.max_deviation);
    }
    if (j.contains("avgDeviation")) {
        parse_stats(j.at("avgDeviation"), a.average_deviation);
    }
    if (j.contains("rmsDeviation")) {
        parse_stats(j.at("rmsDeviation"), a.rms_deviation);
    }

    if (j.contains("triggerPrecision") && j.at("triggerPrecision").is_object()) {
        const auto& tp = j.at("triggerPrecision");
        a.trigger_precision.total_trigger_count = tp.value("totalTriggerCount", 0);
        a.trigger_precision.success_trigger_count = tp.value("successTriggerCount", 0);
        a.trigger_precision.average_trigger_deviation_mm =
            static_cast<Siligen::Shared::Types::float32>(tp.value("averageTriggerDeviationMm", 0.0));
        a.trigger_precision.max_trigger_deviation_mm =
            static_cast<Siligen::Shared::Types::float32>(tp.value("maxTriggerDeviationMm", 0.0));
        a.trigger_precision.trigger_response_time_ms =
            static_cast<Siligen::Shared::Types::float32>(tp.value("triggerResponseTimeMs", 0.0));
    }
}

}  // namespace Siligen::Domain::Motion::ValueObjects

namespace Siligen::Domain::Diagnostics::ValueObjects {
using json = nlohmann::json;

// ========================================
// DiagnosticResult 相关类型 JSON 支持
// ========================================

/**
 * @brief DiagnosticIssue 转 JSON
 */
inline void to_json(json& j, const DiagnosticIssue& i) {
    j = json{
        {"severity", Siligen::Infrastructure::Serialization::ToString(i.severity)},
        {"description", i.description},
        {"suggestedAction", i.suggestedAction}
    };
}

/**
 * @brief JSON 转 DiagnosticIssue
 */
inline void from_json(const json& j, DiagnosticIssue& i) {
    auto severityResult = Siligen::Infrastructure::Serialization::FromString_IssueSeverity(
        j.at("severity").get<std::string>()
    );
    if (severityResult.IsSuccess()) {
        i.severity = severityResult.Value();
    } else {
        i.severity = IssueSeverity::Info;
    }

    i.description = j.at("description").get<std::string>();
    i.suggestedAction = j.at("suggestedAction").get<std::string>();
}

/**
 * @brief HardwareCheckResult 转 JSON
 */
inline void to_json(json& j, const HardwareCheckResult& r) {
    std::vector<int> enabled_axes;
    enabled_axes.reserve(r.enabledAxes.size());
    for (const auto axis_id : r.enabledAxes) {
        enabled_axes.push_back(static_cast<int>(Siligen::Shared::Types::ToIndex(axis_id)));
    }

    std::vector<std::pair<int, bool>> limitSwitchArray;
    for (const auto& kv : r.limitSwitchOk) {
        limitSwitchArray.emplace_back(static_cast<int>(Siligen::Shared::Types::ToIndex(kv.first)), kv.second);
    }

    std::vector<std::pair<int, bool>> encoderArray;
    for (const auto& kv : r.encoderOk) {
        encoderArray.emplace_back(static_cast<int>(Siligen::Shared::Types::ToIndex(kv.first)), kv.second);
    }

    j = json{
        {"controllerConnected", r.controllerConnected},
        {"enabledAxes", enabled_axes},
        {"limitSwitchOk", limitSwitchArray},
        {"encoderOk", encoderArray}
    };
}

/**
 * @brief JSON 转 HardwareCheckResult
 */
inline void from_json(const json& j, HardwareCheckResult& r) {
    r.controllerConnected = j.at("controllerConnected").get<bool>();
    r.enabledAxes.clear();
    if (j.contains("enabledAxes")) {
        const auto enabled_axes = j.at("enabledAxes").get<std::vector<int>>();
        for (const auto axis_value : enabled_axes) {
            const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(axis_value));
            if (Siligen::Shared::Types::IsValid(axis_id)) {
                r.enabledAxes.push_back(axis_id);
            }
        }
    }

    // 从键值对数组重建 std::map<int, bool>
    r.limitSwitchOk.clear();
    if (j.contains("limitSwitchOk") && j["limitSwitchOk"].is_array()) {
        for (const auto& item : j["limitSwitchOk"]) {
            if (item.is_array() && item.size() == 2) {
                int key = item[0].get<int>();
                bool value = item[1].get<bool>();
                const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(key));
                if (Siligen::Shared::Types::IsValid(axis_id)) {
                    r.limitSwitchOk[axis_id] = value;
                }
            }
        }
    }

    r.encoderOk.clear();
    if (j.contains("encoderOk") && j["encoderOk"].is_array()) {
        for (const auto& item : j["encoderOk"]) {
            if (item.is_array() && item.size() == 2) {
                int key = item[0].get<int>();
                bool value = item[1].get<bool>();
                const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(key));
                if (Siligen::Shared::Types::IsValid(axis_id)) {
                    r.encoderOk[axis_id] = value;
                }
            }
        }
    }
}

/**
 * @brief CommunicationCheckResult 转 JSON
 */
inline void to_json(json& j, const CommunicationCheckResult& r) {
    j = json{
        {"avgResponseTimeMs", r.avgResponseTimeMs},
        {"maxResponseTimeMs", r.maxResponseTimeMs},
        {"packetLossRate", r.packetLossRate},
        {"totalCommands", r.totalCommands},
        {"failedCommands", r.failedCommands}
    };
}

/**
 * @brief JSON 转 CommunicationCheckResult
 */
inline void from_json(const json& j, CommunicationCheckResult& r) {
    r.avgResponseTimeMs = j.at("avgResponseTimeMs").get<double>();
    r.maxResponseTimeMs = j.at("maxResponseTimeMs").get<double>();
    r.packetLossRate = j.at("packetLossRate").get<double>();
    r.totalCommands = j.at("totalCommands").get<int>();
    r.failedCommands = j.at("failedCommands").get<int>();
}

/**
 * @brief ResponseTimeCheckResult 转 JSON
 */
inline void to_json(json& j, const ResponseTimeCheckResult& r) {
    // 将 std::map<int, double> 转换为键值对数组
    std::vector<std::pair<int, double>> responseTimeArray;
    for (const auto& kv : r.axisResponseTimeMs) {
        responseTimeArray.emplace_back(static_cast<int>(Siligen::Shared::Types::ToIndex(kv.first)), kv.second);
    }

    j = json{
        {"axisResponseTimeMs", responseTimeArray},
        {"allWithinSpec", r.allWithinSpec},
        {"specLimitMs", r.specLimitMs}
    };
}

/**
 * @brief JSON 转 ResponseTimeCheckResult
 */
inline void from_json(const json& j, ResponseTimeCheckResult& r) {
    // 从键值对数组重建 std::map<int, double>
    r.axisResponseTimeMs.clear();
    if (j.contains("axisResponseTimeMs") && j["axisResponseTimeMs"].is_array()) {
        for (const auto& item : j["axisResponseTimeMs"]) {
            if (item.is_array() && item.size() == 2) {
                int key = item[0].get<int>();
                double value = item[1].get<double>();
                const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(key));
                if (Siligen::Shared::Types::IsValid(axis_id)) {
                    r.axisResponseTimeMs[axis_id] = value;
                }
            }
        }
    }

    r.allWithinSpec = j.at("allWithinSpec").get<bool>();
    r.specLimitMs = j.at("specLimitMs").get<double>();
}

/**
 * @brief AccuracyCheckResult 转 JSON
 */
inline void to_json(json& j, const AccuracyCheckResult& r) {
    // 将 std::map<int, double> 转换为键值对数组
    std::vector<std::pair<int, double>> repeatabilityArray;
    for (const auto& kv : r.repeatabilityError) {
        repeatabilityArray.emplace_back(static_cast<int>(Siligen::Shared::Types::ToIndex(kv.first)), kv.second);
    }

    std::vector<std::pair<int, double>> positioningArray;
    for (const auto& kv : r.positioningError) {
        positioningArray.emplace_back(static_cast<int>(Siligen::Shared::Types::ToIndex(kv.first)), kv.second);
    }

    j = json{
        {"repeatabilityError", repeatabilityArray},
        {"positioningError", positioningArray},
        {"meetsAccuracyRequirement", r.meetsAccuracyRequirement},
        {"requiredAccuracy", r.requiredAccuracy}
    };
}

/**
 * @brief JSON 转 AccuracyCheckResult
 */
inline void from_json(const json& j, AccuracyCheckResult& r) {
    // 从键值对数组重建 std::map<int, double>
    r.repeatabilityError.clear();
    if (j.contains("repeatabilityError") && j["repeatabilityError"].is_array()) {
        for (const auto& item : j["repeatabilityError"]) {
            if (item.is_array() && item.size() == 2) {
                int key = item[0].get<int>();
                double value = item[1].get<double>();
                const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(key));
                if (Siligen::Shared::Types::IsValid(axis_id)) {
                    r.repeatabilityError[axis_id] = value;
                }
            }
        }
    }

    r.positioningError.clear();
    if (j.contains("positioningError") && j["positioningError"].is_array()) {
        for (const auto& item : j["positioningError"]) {
            if (item.is_array() && item.size() == 2) {
                int key = item[0].get<int>();
                double value = item[1].get<double>();
                const auto axis_id = Siligen::Shared::Types::FromIndex(static_cast<int16_t>(key));
                if (Siligen::Shared::Types::IsValid(axis_id)) {
                    r.positioningError[axis_id] = value;
                }
            }
        }
    }

    r.meetsAccuracyRequirement = j.at("meetsAccuracyRequirement").get<bool>();
    r.requiredAccuracy = j.at("requiredAccuracy").get<double>();
}

}  // namespace Siligen::Domain::Diagnostics::ValueObjects




