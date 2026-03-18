#include "domain/diagnostics/domain-services/ProcessResultSerialization.h"

#include "domain/motion/value-objects/HardwareTestTypes.h"
#include "domain/motion/value-objects/TrajectoryAnalysisTypes.h"
#include "domain/motion/value-objects/TrajectoryTypes.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Point.h"
#include "shared/types/SerializationTypes.h"

#include "nlohmann/json.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <string>

namespace {

using json = nlohmann::json;

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::CreateEnumConversionError;
using Siligen::Shared::Types::CreateJsonParseError;
using Siligen::Shared::Types::CreateMissingFieldError;
using Siligen::Shared::Types::CreateSerializationError;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::ToIndex;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::IsValid;

using Siligen::Domain::Motion::ValueObjects::HomingResult;
using Siligen::Domain::Motion::ValueObjects::LimitSwitchState;
using Siligen::Domain::Motion::ValueObjects::TriggerPoint;
using Siligen::Domain::Motion::ValueObjects::TriggerEvent;
using Siligen::Domain::Motion::ValueObjects::TriggerAction;
using Siligen::Domain::Motion::ValueObjects::TriggerType;
using Siligen::Domain::Motion::ValueObjects::JogDirection;
using Siligen::Domain::Motion::ValueObjects::HomingTestMode;
using Siligen::Domain::Motion::ValueObjects::HomingTestDirection;
using Siligen::Domain::Motion::ValueObjects::TrajectoryType;
using Siligen::Domain::Motion::ValueObjects::TrajectoryInterpolationType;
using Siligen::Domain::Motion::ValueObjects::CMPParameters;
using Siligen::Domain::Motion::ValueObjects::InterpolationParameters;
using Siligen::Domain::Motion::ValueObjects::PathQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::MotionQualityMetrics;
using Siligen::Domain::Motion::ValueObjects::TrajectoryAnalysis;
using Siligen::Domain::Motion::ValueObjects::DeviationStatistics;
using Siligen::Domain::Motion::ValueObjects::TriggerPrecisionAnalysis;

using Siligen::Domain::Diagnostics::ValueObjects::TriggerStatistics;
using Siligen::Domain::Diagnostics::ValueObjects::DiagnosticIssue;
using Siligen::Domain::Diagnostics::ValueObjects::IssueSeverity;
using Siligen::Domain::Diagnostics::ValueObjects::HardwareCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::CommunicationCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::ResponseTimeCheckResult;
using Siligen::Domain::Diagnostics::ValueObjects::AccuracyCheckResult;

Result<json> ParseJson(const std::string& json_text) noexcept {
    json parsed = json::parse(json_text, nullptr, false);
    if (parsed.is_discarded()) {
        return Result<json>::Failure(CreateJsonParseError("JSON parse error: invalid JSON"));
    }
    return Result<json>::Success(std::move(parsed));
}

Result<std::string> DumpJson(const json& j) noexcept {
    return Result<std::string>::Success(j.dump(4, ' ', false, json::error_handler_t::replace));
}

Result<int> GetRequiredInt(const json& j, const char* field) noexcept {
    if (!j.contains(field)) {
        return Result<int>::Failure(CreateMissingFieldError(field));
    }
    const auto& value = j.at(field);
    if (const auto* int_ptr = value.get_ptr<const json::number_integer_t*>()) {
        if (*int_ptr < std::numeric_limits<int>::min() || *int_ptr > std::numeric_limits<int>::max()) {
            return Result<int>::Failure(CreateSerializationError(
                ErrorCode::JSON_INVALID_TYPE, std::string("Field ") + field + " is out of range"));
        }
        return Result<int>::Success(static_cast<int>(*int_ptr));
    }
    if (const auto* uint_ptr = value.get_ptr<const json::number_unsigned_t*>()) {
        if (*uint_ptr > static_cast<json::number_unsigned_t>(std::numeric_limits<int>::max())) {
            return Result<int>::Failure(CreateSerializationError(
                ErrorCode::JSON_INVALID_TYPE, std::string("Field ") + field + " is out of range"));
        }
        return Result<int>::Success(static_cast<int>(*uint_ptr));
    }
    return Result<int>::Failure(CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                                         std::string("Field ") + field + " is not an integer"));
}

Result<double> GetRequiredDouble(const json& j, const char* field) noexcept {
    if (!j.contains(field)) {
        return Result<double>::Failure(CreateMissingFieldError(field));
    }
    const auto& value = j.at(field);
    if (const auto* float_ptr = value.get_ptr<const json::number_float_t*>()) {
        return Result<double>::Success(*float_ptr);
    }
    if (const auto* int_ptr = value.get_ptr<const json::number_integer_t*>()) {
        return Result<double>::Success(static_cast<double>(*int_ptr));
    }
    if (const auto* uint_ptr = value.get_ptr<const json::number_unsigned_t*>()) {
        return Result<double>::Success(static_cast<double>(*uint_ptr));
    }
    return Result<double>::Failure(CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                                            std::string("Field ") + field + " is not a number"));
}

Result<std::string> GetRequiredString(const json& j, const char* field) noexcept {
    if (!j.contains(field)) {
        return Result<std::string>::Failure(CreateMissingFieldError(field));
    }
    const auto& value = j.at(field);
    if (const auto* str_ptr = value.get_ptr<const std::string*>()) {
        return Result<std::string>::Success(*str_ptr);
    }
    return Result<std::string>::Failure(CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                                                 std::string("Field ") + field + " is not a string"));
}

Result<LogicalAxisId> AxisFromIndex(int index) noexcept {
    const auto axis = FromIndex(static_cast<int16_t>(index));
    if (!IsValid(axis)) {
        return Result<LogicalAxisId>::Failure(Error(ErrorCode::INVALID_PARAMETER,
                                                    std::string("Invalid axis index: ") + std::to_string(index),
                                                    "DiagnosticsSerialization"));
    }
    return Result<LogicalAxisId>::Success(axis);
}

const char* ToString(JogDirection direction) noexcept {
    switch (direction) {
        case JogDirection::Positive:
            return "Positive";
        case JogDirection::Negative:
            return "Negative";
        default:
            return "Unknown";
    }
}

Result<JogDirection> FromStringJogDirection(const std::string& value) noexcept {
    if (value == "Positive") {
        return Result<JogDirection>::Success(JogDirection::Positive);
    }
    if (value == "Negative") {
        return Result<JogDirection>::Success(JogDirection::Negative);
    }
    return Result<JogDirection>::Failure(CreateEnumConversionError("JogDirection", value));
}

const char* ToString(HomingTestMode mode) noexcept {
    switch (mode) {
        case HomingTestMode::SingleAxis:
            return "SingleAxis";
        case HomingTestMode::MultiAxisSync:
            return "MultiAxisSync";
        case HomingTestMode::Sequential:
            return "Sequential";
        default:
            return "Unknown";
    }
}

const char* ToString(HomingTestDirection direction) noexcept {
    switch (direction) {
        case HomingTestDirection::Positive:
            return "Positive";
        case HomingTestDirection::Negative:
            return "Negative";
        default:
            return "Unknown";
    }
}

const char* ToString(TriggerAction action) noexcept {
    switch (action) {
        case TriggerAction::TurnOn:
            return "TurnOn";
        case TriggerAction::TurnOff:
            return "TurnOff";
        default:
            return "Unknown";
    }
}

const char* ToString(TriggerType type) noexcept {
    switch (type) {
        case TriggerType::SinglePoint:
            return "SinglePoint";
        case TriggerType::MultiPoint:
            return "MultiPoint";
        case TriggerType::ZoneTrigger:
            return "ZoneTrigger";
        default:
            return "Unknown";
    }
}

const char* ToString(TrajectoryType type) noexcept {
    switch (type) {
        case TrajectoryType::Linear:
            return "Linear";
        case TrajectoryType::CircularArc:
            return "CircularArc";
        case TrajectoryType::BezierCurve:
            return "BezierCurve";
        case TrajectoryType::BSpline:
            return "BSpline";
        default:
            return "Unknown";
    }
}

const char* ToString(TrajectoryInterpolationType type) noexcept {
    switch (type) {
        case TrajectoryInterpolationType::Linear:
            return "Linear";
        case TrajectoryInterpolationType::CircularArc:
            return "CircularArc";
        case TrajectoryInterpolationType::BSpline:
            return "BSpline";
        case TrajectoryInterpolationType::Bezier:
            return "Bezier";
        default:
            return "Unknown";
    }
}

const char* ToString(IssueSeverity severity) noexcept {
    switch (severity) {
        case IssueSeverity::Info:
            return "Info";
        case IssueSeverity::Warning:
            return "Warning";
        case IssueSeverity::Error:
            return "Error";
        case IssueSeverity::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

json Point2DToJson(const Siligen::Point2D& p) {
    return json{{"x", p.x}, {"y", p.y}};
}

json HomingResultToJson(const HomingResult& result) {
    json j = json{{"success", result.success},
                  {"durationMs", result.durationMs},
                  {"finalPosition", result.finalPosition}};
    if (result.failureReason.has_value()) {
        j["failureReason"] = result.failureReason.value();
    } else {
        j["failureReason"] = nullptr;
    }
    return j;
}

json LimitSwitchStateToJson(const LimitSwitchState& state) {
    return json{{"positiveLimitTriggered", state.positiveLimitTriggered},
                {"negativeLimitTriggered", state.negativeLimitTriggered},
                {"timestamp", state.timestamp}};
}

json TriggerPointToJson(const TriggerPoint& point) {
    return json{{"axis", static_cast<int>(ToIndex(point.axis))},
                {"position", point.position},
                {"outputPort", point.outputPort},
                {"action", ::ToString(point.action)}};
}

json TriggerEventToJson(const TriggerEvent& event) {
    return json{{"timestamp", event.timestamp},
                {"triggerPointId", event.triggerPointId},
                {"actualPosition", event.actualPosition},
                {"positionError", event.positionError},
                {"responseTimeUs", event.responseTimeUs},
                {"outputVerified", event.outputVerified}};
}

json TriggerStatisticsToJson(const TriggerStatistics& stats) {
    return json{{"maxPositionError", stats.maxPositionError},
                {"avgPositionError", stats.avgPositionError},
                {"maxResponseTimeUs", stats.maxResponseTimeUs},
                {"avgResponseTimeUs", stats.avgResponseTimeUs},
                {"successfulTriggers", stats.successfulTriggers},
                {"missedTriggers", stats.missedTriggers}};
}

json CMPParametersToJson(const CMPParameters& params) {
    return json{{"compensationGain", params.compensationGain},
                {"lookaheadPoints", params.lookaheadPoints},
                {"maxCompensation", params.maxCompensation}};
}

json InterpolationParametersToJson(const InterpolationParameters& params) {
    return json{{"targetFeedRate", params.targetFeedRate},
                {"acceleration", params.acceleration},
                {"interpolationCycle", params.interpolationCycle}};
}

json PathQualityMetricsToJson(const PathQualityMetrics& metrics) {
    return json{{"maxPathError", metrics.maxPathError},
                {"avgPathError", metrics.avgPathError},
                {"circularityError", metrics.circularityError},
                {"radiusError", metrics.radiusError}};
}

json MotionQualityMetricsToJson(const MotionQualityMetrics& metrics) {
    return json{{"maxVelocity", metrics.maxVelocity},
                {"avgVelocity", metrics.avgVelocity},
                {"maxAcceleration", metrics.maxAcceleration},
                {"accelerationDiscontinuities", metrics.accelerationDiscontinuities},
                {"velocitySmoothness", metrics.velocitySmoothness}};
}

json DeviationStatisticsToJson(const DeviationStatistics& stats) {
    return json{{"positionDeviationMm", stats.position_deviation_mm},
                {"timeDeviationMs", stats.time_deviation_ms},
                {"velocityDeviationPercent", stats.velocity_deviation_percent}};
}

json TriggerPrecisionToJson(const TriggerPrecisionAnalysis& analysis) {
    return json{{"totalTriggerCount", analysis.total_trigger_count},
                {"successTriggerCount", analysis.success_trigger_count},
                {"averageTriggerDeviationMm", analysis.average_trigger_deviation_mm},
                {"maxTriggerDeviationMm", analysis.max_trigger_deviation_mm},
                {"triggerResponseTimeMs", analysis.trigger_response_time_ms}};
}

json TrajectoryAnalysisToJson(const TrajectoryAnalysis& analysis) {
    return json{{"maxDeviation", DeviationStatisticsToJson(analysis.max_deviation)},
                {"avgDeviation", DeviationStatisticsToJson(analysis.average_deviation)},
                {"rmsDeviation", DeviationStatisticsToJson(analysis.rms_deviation)},
                {"triggerPrecision", TriggerPrecisionToJson(analysis.trigger_precision)}};
}

json DiagnosticIssueToJson(const DiagnosticIssue& issue) {
    return json{{"severity", ::ToString(issue.severity)},
                {"description", issue.description},
                {"suggestedAction", issue.suggestedAction}};
}

json AxisBoolMapToJson(const std::map<LogicalAxisId, bool>& map) {
    json items = json::array();
    for (const auto& kv : map) {
        items.push_back(json::array({static_cast<int>(ToIndex(kv.first)), kv.second}));
    }
    return items;
}

json AxisDoubleMapToJson(const std::map<LogicalAxisId, double>& map) {
    json items = json::array();
    for (const auto& kv : map) {
        items.push_back(json::array({static_cast<int>(ToIndex(kv.first)), kv.second}));
    }
    return items;
}

json HardwareCheckToJson(const HardwareCheckResult& result) {
    json enabled_axes = json::array();
    for (const auto axis : result.enabledAxes) {
        enabled_axes.push_back(static_cast<int>(ToIndex(axis)));
    }

    return json{{"controllerConnected", result.controllerConnected},
                {"enabledAxes", enabled_axes},
                {"limitSwitchOk", AxisBoolMapToJson(result.limitSwitchOk)},
                {"encoderOk", AxisBoolMapToJson(result.encoderOk)}};
}

json CommunicationCheckToJson(const CommunicationCheckResult& result) {
    return json{{"avgResponseTimeMs", result.avgResponseTimeMs},
                {"maxResponseTimeMs", result.maxResponseTimeMs},
                {"packetLossRate", result.packetLossRate},
                {"totalCommands", result.totalCommands},
                {"failedCommands", result.failedCommands}};
}

json ResponseTimeCheckToJson(const ResponseTimeCheckResult& result) {
    return json{{"axisResponseTimeMs", AxisDoubleMapToJson(result.axisResponseTimeMs)},
                {"allWithinSpec", result.allWithinSpec},
                {"specLimitMs", result.specLimitMs}};
}

json AccuracyCheckToJson(const AccuracyCheckResult& result) {
    return json{{"repeatabilityError", AxisDoubleMapToJson(result.repeatabilityError)},
                {"positioningError", AxisDoubleMapToJson(result.positioningError)},
                {"meetsAccuracyRequirement", result.meetsAccuracyRequirement},
                {"requiredAccuracy", result.requiredAccuracy}};
}

}  // namespace

namespace Siligen::Domain::Diagnostics::Serialization {

Result<std::string> SerializeJogParameters(const JogTestData& data) noexcept {
    json j{{"axis", static_cast<int>(ToIndex(data.axis))},
           {"direction", ::ToString(data.direction)},
           {"speed", data.speed},
           {"distance", data.distance}};
    return DumpJson(j);
}

Result<std::string> SerializeJogResult(const JogTestData& data) noexcept {
    json j{{"startPosition", data.startPosition},
           {"endPosition", data.endPosition},
           {"actualDistance", data.actualDistance},
           {"responseTimeMs", data.responseTimeMs}};
    return DumpJson(j);
}

Result<JogTestData> DeserializeJogTestData(const std::string& parameters_json,
                                           const std::string& result_json) noexcept {
    auto params_result = ParseJson(parameters_json);
    if (params_result.IsError()) {
        return Result<JogTestData>::Failure(params_result.GetError());
    }
    auto results_result = ParseJson(result_json);
    if (results_result.IsError()) {
        return Result<JogTestData>::Failure(results_result.GetError());
    }

    const auto& params = params_result.Value();
    const auto& results = results_result.Value();

    auto axis_index = GetRequiredInt(params, "axis");
    if (axis_index.IsError()) {
        return Result<JogTestData>::Failure(axis_index.GetError());
    }
    auto axis_id = AxisFromIndex(axis_index.Value());
    if (axis_id.IsError()) {
        return Result<JogTestData>::Failure(axis_id.GetError());
    }

    auto direction_str = GetRequiredString(params, "direction");
    if (direction_str.IsError()) {
        return Result<JogTestData>::Failure(direction_str.GetError());
    }
    auto direction = ::FromStringJogDirection(direction_str.Value());
    if (direction.IsError()) {
        return Result<JogTestData>::Failure(direction.GetError());
    }

    auto speed = GetRequiredDouble(params, "speed");
    if (speed.IsError()) {
        return Result<JogTestData>::Failure(speed.GetError());
    }
    auto distance = GetRequiredDouble(params, "distance");
    if (distance.IsError()) {
        return Result<JogTestData>::Failure(distance.GetError());
    }

    auto start_pos = GetRequiredDouble(results, "startPosition");
    if (start_pos.IsError()) {
        return Result<JogTestData>::Failure(start_pos.GetError());
    }
    auto end_pos = GetRequiredDouble(results, "endPosition");
    if (end_pos.IsError()) {
        return Result<JogTestData>::Failure(end_pos.GetError());
    }
    auto actual_distance = GetRequiredDouble(results, "actualDistance");
    if (actual_distance.IsError()) {
        return Result<JogTestData>::Failure(actual_distance.GetError());
    }
    auto response_time = GetRequiredInt(results, "responseTimeMs");
    if (response_time.IsError()) {
        return Result<JogTestData>::Failure(response_time.GetError());
    }

    JogTestData data;
    data.axis = axis_id.Value();
    data.direction = direction.Value();
    data.speed = speed.Value();
    data.distance = distance.Value();
    data.startPosition = start_pos.Value();
    data.endPosition = end_pos.Value();
    data.actualDistance = actual_distance.Value();
    data.responseTimeMs = static_cast<std::int32_t>(response_time.Value());
    return Result<JogTestData>::Success(data);
}

Result<std::string> SerializeHomingParameters(const HomingTestData& data) noexcept {
    json axes = json::array();
    for (const auto axis : data.axes) {
        axes.push_back(static_cast<int>(ToIndex(axis)));
    }

    json j{{"axes", axes},
           {"mode", ::ToString(data.mode)},
           {"searchSpeed", data.searchSpeed},
           {"returnSpeed", data.returnSpeed},
           {"direction", ::ToString(data.direction)}};
    return DumpJson(j);
}

Result<std::string> SerializeHomingResult(const HomingTestData& data) noexcept {
    json axis_results = json::array();
    for (const auto& kv : data.axisResults) {
        axis_results.push_back(json::array({static_cast<int>(ToIndex(kv.first)), HomingResultToJson(kv.second)}));
    }

    json limit_states = json::array();
    for (const auto& kv : data.limitStates) {
        limit_states.push_back(json::array({static_cast<int>(ToIndex(kv.first)), LimitSwitchStateToJson(kv.second)}));
    }

    json j{{"axisResults", axis_results}, {"limitStates", limit_states}};
    return DumpJson(j);
}

Result<std::string> SerializeTriggerParameters(const TriggerTestData& data) noexcept {
    json points = json::array();
    for (const auto& point : data.triggerPoints) {
        points.push_back(TriggerPointToJson(point));
    }

    json j{{"triggerType", ::ToString(data.triggerType)}, {"triggerPoints", points}};
    return DumpJson(j);
}

Result<std::string> SerializeTriggerResult(const TriggerTestData& data) noexcept {
    json events = json::array();
    for (const auto& event : data.triggerEvents) {
        events.push_back(TriggerEventToJson(event));
    }

    json j{{"triggerEvents", events}, {"statistics", TriggerStatisticsToJson(data.statistics)}};
    return DumpJson(j);
}

Result<std::string> SerializeCMPParameters(const CMPTestData& data) noexcept {
    json points = json::array();
    for (const auto& point : data.controlPoints) {
        points.push_back(Point2DToJson(point));
    }

    json j{{"trajectoryType", ::ToString(data.trajectoryType)},
           {"controlPoints", points},
           {"cmpParams", CMPParametersToJson(data.cmpParams)}};
    return DumpJson(j);
}

Result<std::string> SerializeCMPResult(const CMPTestData& data) noexcept {
    json theoretical = json::array();
    for (const auto& point : data.theoreticalPath) {
        theoretical.push_back(Point2DToJson(point));
    }
    json actual = json::array();
    for (const auto& point : data.actualPath) {
        actual.push_back(Point2DToJson(point));
    }
    json compensation = json::array();
    for (const auto& point : data.compensationData) {
        compensation.push_back(Point2DToJson(point));
    }

    json j{{"theoreticalPath", theoretical},
           {"actualPath", actual},
           {"compensationData", compensation},
           {"analysis", TrajectoryAnalysisToJson(data.analysis)}};
    return DumpJson(j);
}

Result<std::string> SerializeInterpolationParameters(const InterpolationTestData& data) noexcept {
    json points = json::array();
    for (const auto& point : data.controlPoints) {
        points.push_back(Point2DToJson(point));
    }

    json j{{"interpolationType", ::ToString(data.interpolationType)},
           {"controlPoints", points},
           {"interpParams", InterpolationParametersToJson(data.interpParams)}};
    return DumpJson(j);
}

Result<std::string> SerializeInterpolationResult(const InterpolationTestData& data) noexcept {
    json interpolated = json::array();
    for (const auto& point : data.interpolatedPath) {
        interpolated.push_back(Point2DToJson(point));
    }

    json j{{"interpolatedPath", interpolated},
           {"pathQuality", PathQualityMetricsToJson(data.pathQuality)},
           {"motionQuality", MotionQualityMetricsToJson(data.motionQuality)}};
    return DumpJson(j);
}

Result<std::string> SerializeDiagnosticResult(const DiagnosticResult& data) noexcept {
    json issues = json::array();
    for (const auto& issue : data.issues) {
        issues.push_back(DiagnosticIssueToJson(issue));
    }

    json j{{"hardwareCheck", HardwareCheckToJson(data.hardwareCheck)},
           {"commCheck", CommunicationCheckToJson(data.commCheck)},
           {"responseCheck", ResponseTimeCheckToJson(data.responseCheck)},
           {"accuracyCheck", AccuracyCheckToJson(data.accuracyCheck)},
           {"healthScore", data.healthScore},
           {"issues", issues}};

    return DumpJson(j);
}

}  // namespace Siligen::Domain::Diagnostics::Serialization
