// CMPTriggerService.cpp - Minimal stub for compilation
#include "CMPTriggerService.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <sstream>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace DomainServices {

using namespace Siligen::Shared::Types;
using namespace Siligen::Shared::Interfaces;
using json = nlohmann::json;

namespace {
constexpr auto kDefaultTriggerAxis = Siligen::Shared::Types::LogicalAxisId::X;
constexpr float kDistanceEpsilon = 1e-4f;

json TriggerPointToJson(const CMPTriggerPoint& point) {
    return json{
        {"position", point.position},
        {"action", static_cast<int>(point.action)},
        {"pulse_width_us", point.pulse_width_us},
        {"delay_time_us", point.delay_time_us},
        {"is_enabled", point.is_enabled},
    };
}

Result<CMPTriggerPoint> TriggerPointFromJson(const json& value) {
    if (!value.is_object()) {
        return Result<CMPTriggerPoint>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, "trigger point must be an object", "CMPService"));
    }

    if (!value.contains("position") || !value.contains("action")) {
        return Result<CMPTriggerPoint>::Failure(
            Error(ErrorCode::JSON_MISSING_REQUIRED_FIELD, "trigger point missing required fields", "CMPService"));
    }

    CMPTriggerPoint point;
    point.position = value.at("position").get<int32_t>();
    point.action = static_cast<DispensingAction>(value.at("action").get<int>());
    point.pulse_width_us = value.value("pulse_width_us", 2000);
    point.delay_time_us = value.value("delay_time_us", 0);
    point.is_enabled = value.value("is_enabled", true);
    return Result<CMPTriggerPoint>::Success(point);
}
}

CMPService::CMPService(std::shared_ptr<Siligen::Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
                       std::shared_ptr<ILoggingService> logging_service)
    : trigger_port_(std::move(trigger_port)), logging_service_(std::move(logging_service)) {}

Result<void> CMPService::ConfigureCMP(const CMPConfiguration& config) {
    // TODO: Phase 6 - CMP硬件配置实现
    // 需要通过硬件端口接口将CMP配置写入MultiCard寄存器：
    // 1. 设置CMP触发源（编码器位置 vs 定时器）
    // 2. 配置比较输出通道
    // 3. 写入trigger_points到CMP寄存器
    // 4. 设置pulse_width_us到I/O输出配置
    // 5. 启用CMP功能模块
    //
    // 当前版本: 仅进行配置验证，不执行硬件配置
    // 电机运动已在CMPTestUseCase::collectActualPath()中实现
    // CMP触发器配置留待后续Phase完整实现

    // 验证配置有效性
    auto validationResult = ValidateCMPConfiguration(config);
    if (!validationResult.IsSuccess()) {
        return validationResult;
    }

    // 记录配置（用于调试）
    if (logging_service_) {
        std::string log_msg = "CMP配置已验证: trigger_mode=" + std::to_string(static_cast<int>(config.trigger_mode)) +
                              ", trigger_points=" + std::to_string(config.trigger_points.size());
        logging_service_->LogInfo("CMPService", log_msg);
    }

    cmp_configuration_ = config;
    return Result<void>::Success();
}

const CMPConfiguration& CMPService::GetCMPConfiguration() const {
    return cmp_configuration_;
}

Result<void> CMPService::EnableCMP() {
    cmp_configuration_.is_enabled = true;
    return Result<void>::Success();
}

Result<void> CMPService::DisableCMP() {
    if (trigger_port_ && cmp_configuration_.is_enabled) {
        auto disable_result = trigger_port_->DisableTrigger(kDefaultTriggerAxis);
        if (disable_result.IsError() &&
            disable_result.GetError().GetCode() != ErrorCode::INVALID_STATE &&
            disable_result.GetError().GetCode() != ErrorCode::NOT_IMPLEMENTED) {
            return disable_result;
        }
    }

    cmp_configuration_.is_enabled = false;
    return Result<void>::Success();
}

Result<bool> CMPService::IsCMPEnabled() const {
    if (trigger_port_ && cmp_configuration_.is_enabled) {
        auto status_result = trigger_port_->IsTriggerEnabled(kDefaultTriggerAxis);
        if (status_result.IsSuccess()) {
            return status_result;
        }
    }

    return Result<bool>::Success(cmp_configuration_.is_enabled);
}

Result<void> CMPService::ClearAllTriggerPoints() {
    if (trigger_port_ && !cmp_configuration_.trigger_points.empty()) {
        auto clear_result = trigger_port_->ClearTrigger(kDefaultTriggerAxis);
        if (clear_result.IsError() &&
            clear_result.GetError().GetCode() != ErrorCode::INVALID_STATE &&
            clear_result.GetError().GetCode() != ErrorCode::NOT_IMPLEMENTED) {
            return clear_result;
        }
    }

    cmp_configuration_.ClearTriggerPoints();
    cmp_configuration_.is_enabled = false;
    return Result<void>::Success();
}

Result<void> CMPService::AddTriggerPoint(const CMPTriggerPoint& trigger_point) {
    auto validation = ValidateTriggerPoint(trigger_point);
    if (!validation.IsSuccess()) {
        return validation;
    }

    cmp_configuration_.AddTriggerPoint(trigger_point);
    SortTriggerPoints();
    return Result<void>::Success();
}

Result<void> CMPService::AddTriggerPoints(const std::vector<CMPTriggerPoint>& trigger_points) {
    auto count_check = ValidateTriggerPointCount(static_cast<int32_t>(trigger_points.size()));
    if (!count_check.IsSuccess()) {
        return count_check;
    }

    for (const auto& point : trigger_points) {
        auto validation = ValidateTriggerPoint(point);
        if (!validation.IsSuccess()) {
            return validation;
        }
    }

    cmp_configuration_.trigger_points.insert(
        cmp_configuration_.trigger_points.end(),
        trigger_points.begin(),
        trigger_points.end());
    SortTriggerPoints();
    return Result<void>::Success();
}

Result<void> CMPService::RemoveTriggerPoint(int32_t position) {
    auto it = std::find_if(cmp_configuration_.trigger_points.begin(),
                           cmp_configuration_.trigger_points.end(),
                           [&](const CMPTriggerPoint& point) { return point.position == position; });
    if (it == cmp_configuration_.trigger_points.end()) {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_FOUND, "Trigger point not found", "CMPService"));
    }
    cmp_configuration_.trigger_points.erase(it);
    return Result<void>::Success();
}

Result<CMPTriggerPoint> CMPService::GetTriggerPoint(int32_t position) const {
    auto it = std::find_if(cmp_configuration_.trigger_points.begin(),
                           cmp_configuration_.trigger_points.end(),
                           [&](const CMPTriggerPoint& point) { return point.position == position; });
    if (it == cmp_configuration_.trigger_points.end()) {
        return Result<CMPTriggerPoint>::Failure(
            Error(ErrorCode::NOT_FOUND, "Trigger point not found", "CMPService"));
    }
    return Result<CMPTriggerPoint>::Success(*it);
}

Result<std::vector<CMPTriggerPoint>> CMPService::GetAllTriggerPoints() const {
    return Result<std::vector<CMPTriggerPoint>>::Success(cmp_configuration_.trigger_points);
}

Result<int32_t> CMPService::GetTriggerPointCount() const {
    return Result<int32_t>::Success(static_cast<int32_t>(cmp_configuration_.trigger_points.size()));
}

Result<void> CMPService::SetTriggerMode(CMPTriggerMode mode) {
    cmp_configuration_.trigger_mode = mode;
    return Result<void>::Success();
}

Result<CMPTriggerMode> CMPService::GetTriggerMode() const {
    return Result<CMPTriggerMode>::Success(cmp_configuration_.trigger_mode);
}

Result<void> CMPService::SetTriggerRange(int32_t start_position, int32_t end_position) {
    if (start_position < 0 || end_position < 0 || start_position >= end_position) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid trigger range", "CMPService"));
    }
    cmp_configuration_.start_position = start_position;
    cmp_configuration_.end_position = end_position;
    return Result<void>::Success();
}

Result<void> CMPService::ValidateCMPConfiguration(const CMPConfiguration& config) const {
    // 基础数据验证 (调用CMPConfiguration自带的验证)
    std::string error_detail;
    if (!config.Validate(&error_detail)) {
        return Result<void>::Failure(Error(ErrorCode::ValidationFailed, "CMP配置基础验证失败: " + error_detail));
    }

    // 业务规则验证：SEQUENCE模式必须至少有一个触发点
    if (config.trigger_mode == CMPTriggerMode::SEQUENCE && config.trigger_points.empty()) {
        return Result<void>::Failure(Error(ErrorCode::ValidationFailed, "SEQUENCE模式必须至少有一个触发点"));
    }

    // 业务规则验证：SINGLE_POINT模式必须有一个触发点
    if (config.trigger_mode == CMPTriggerMode::SINGLE_POINT && config.trigger_points.size() != 1) {
        return Result<void>::Failure(Error(ErrorCode::ValidationFailed, "SINGLE_POINT模式必须有且仅有一个触发点"));
    }

    return Result<void>::Success();
}

Result<void> CMPService::ValidateTriggerPoint(const CMPTriggerPoint& trigger_point) const {
    if (!trigger_point.Validate()) {
        return Result<void>::Failure(
            Error(ErrorCode::ValidationFailed, "Trigger point validation failed", "CMPService"));
    }
    return Result<void>::Success();
}

Result<void> CMPService::ValidateTriggerPointCount(int32_t count) const {
    if (count < MIN_TRIGGER_POINTS || count > MAX_TRIGGER_POINTS) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Trigger point count out of range", "CMPService"));
    }
    return Result<void>::Success();
}

Result<bool> CMPService::HasTriggerPointConflict(int32_t position) const {
    bool conflict = std::any_of(cmp_configuration_.trigger_points.begin(),
                                cmp_configuration_.trigger_points.end(),
                                [&](const CMPTriggerPoint& point) { return point.position == position; });
    return Result<bool>::Success(conflict);
}

Result<std::vector<CMPTriggerPoint>> CMPService::GenerateTriggerPointsFromPath(
    const std::vector<Point2D>& path, float interval, int32_t pulse_width) const {
    if (path.size() < 2) {
        return Result<std::vector<CMPTriggerPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Path must contain at least 2 points", "CMPService"));
    }
    if (interval <= 0.0f) {
        return Result<std::vector<CMPTriggerPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Interval must be greater than 0", "CMPService"));
    }
    if (pulse_width < MIN_PULSE_WIDTH || pulse_width > MAX_PULSE_WIDTH) {
        return Result<std::vector<CMPTriggerPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Pulse width out of range", "CMPService"));
    }

    std::vector<CMPTriggerPoint> trigger_points;
    float cumulative_length = 0.0f;
    float next_trigger_distance = interval;

    for (size_t i = 1; i < path.size(); ++i) {
        const float segment_length = path[i - 1].DistanceTo(path[i]);
        if (segment_length <= kDistanceEpsilon) {
            continue;
        }

        while (next_trigger_distance <= cumulative_length + segment_length + kDistanceEpsilon) {
            const int32_t position = static_cast<int32_t>(std::lround(next_trigger_distance));
            if (trigger_points.empty() || trigger_points.back().position != position) {
                trigger_points.emplace_back(position, DispensingAction::PULSE, pulse_width, 0);
            }
            next_trigger_distance += interval;
        }

        cumulative_length += segment_length;
    }

    const int32_t final_position = static_cast<int32_t>(std::lround(cumulative_length));
    if (final_position > 0 &&
        (trigger_points.empty() || trigger_points.back().position != final_position)) {
        trigger_points.emplace_back(final_position, DispensingAction::PULSE, pulse_width, 0);
    }

    if (trigger_points.empty()) {
        return Result<std::vector<CMPTriggerPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "No trigger points generated from path", "CMPService"));
    }

    return Result<std::vector<CMPTriggerPoint>>::Success(std::move(trigger_points));
}

Result<void> CMPService::OptimizeTriggerPoints() {
    SortTriggerPoints();
    auto& points = cmp_configuration_.trigger_points;
    points.erase(
        std::unique(points.begin(),
                    points.end(),
                    [](const CMPTriggerPoint& lhs, const CMPTriggerPoint& rhs) {
                        return lhs.position == rhs.position &&
                               lhs.action == rhs.action &&
                               lhs.pulse_width_us == rhs.pulse_width_us &&
                               lhs.delay_time_us == rhs.delay_time_us &&
                               lhs.is_enabled == rhs.is_enabled;
                    }),
        points.end());
    return Result<void>::Success();
}

Result<std::string> CMPService::ExportConfigurationToJson() const {
    json payload;
    payload["trigger_mode"] = static_cast<int>(cmp_configuration_.trigger_mode);
    payload["start_position"] = cmp_configuration_.start_position;
    payload["end_position"] = cmp_configuration_.end_position;
    payload["is_enabled"] = cmp_configuration_.is_enabled;
    payload["cmp_channel"] = cmp_configuration_.cmp_channel;
    payload["pulse_width_us"] = cmp_configuration_.pulse_width_us;
    payload["enable_compensation"] = cmp_configuration_.enable_compensation;
    payload["compensation_factor"] = cmp_configuration_.compensation_factor;
    payload["trigger_position_tolerance"] = cmp_configuration_.trigger_position_tolerance;
    payload["time_tolerance_ms"] = cmp_configuration_.time_tolerance_ms;
    payload["enable_multi_channel"] = cmp_configuration_.enable_multi_channel;
    payload["channel_mapping"] = cmp_configuration_.channel_mapping;
    payload["trigger_points"] = json::array();

    for (const auto& point : cmp_configuration_.trigger_points) {
        payload["trigger_points"].push_back(TriggerPointToJson(point));
    }

    return Result<std::string>::Success(payload.dump());
}

Result<void> CMPService::ImportConfigurationFromJson(const std::string& json_config) {
    json parsed = json::parse(json_config, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return Result<void>::Failure(
            Error(ErrorCode::JSON_PARSE_ERROR, "Invalid CMP configuration json", "CMPService"));
    }

    CMPConfiguration imported;
    imported.trigger_mode = static_cast<CMPTriggerMode>(parsed.value("trigger_mode", static_cast<int>(CMPTriggerMode::SINGLE_POINT)));
    imported.start_position = parsed.value("start_position", 0);
    imported.end_position = parsed.value("end_position", 0);
    imported.is_enabled = parsed.value("is_enabled", false);
    imported.cmp_channel = parsed.value("cmp_channel", 1);
    imported.pulse_width_us = parsed.value("pulse_width_us", 2000);
    imported.enable_compensation = parsed.value("enable_compensation", false);
    imported.compensation_factor = parsed.value("compensation_factor", 1.0f);
    imported.trigger_position_tolerance = parsed.value("trigger_position_tolerance", 0.1f);
    imported.time_tolerance_ms = parsed.value("time_tolerance_ms", 1.0f);
    imported.enable_multi_channel = parsed.value("enable_multi_channel", false);
    imported.channel_mapping = parsed.value("channel_mapping", std::vector<int32>{});

    if (parsed.contains("trigger_points")) {
        if (!parsed.at("trigger_points").is_array()) {
            return Result<void>::Failure(
                Error(ErrorCode::JSON_INVALID_TYPE, "trigger_points must be an array", "CMPService"));
        }

        for (const auto& point_value : parsed.at("trigger_points")) {
            auto point_result = TriggerPointFromJson(point_value);
            if (point_result.IsError()) {
                return Result<void>::Failure(point_result.GetError());
            }
            imported.trigger_points.push_back(point_result.Value());
        }
    }

    auto validation = ValidateCMPConfiguration(imported);
    if (!validation.IsSuccess()) {
        return validation;
    }

    cmp_configuration_ = std::move(imported);
    OptimizeTriggerPoints();
    return Result<void>::Success();
}

void CMPService::RegisterEventCallback(EventCallback callback) {
    event_callbacks_.push_back(std::move(callback));
}

void CMPService::ClearEventCallbacks() {
    event_callbacks_.clear();
}

void CMPService::PublishEvent(const Siligen::Shared::Types::DomainEvent& event) {
    for (const auto& callback : event_callbacks_) {
        if (callback) {
            callback(event);
        }
    }
}

void CMPService::SortTriggerPoints() {
    std::sort(cmp_configuration_.trigger_points.begin(),
              cmp_configuration_.trigger_points.end(),
              [](const CMPTriggerPoint& a, const CMPTriggerPoint& b) {
                  return a.position < b.position;
              });
}

void CMPService::LogInfo(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogInfo("CMPService", message);
    }
}

void CMPService::LogError(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogError("CMPService", message);
    }
}

void CMPService::LogDebug(const std::string& message) {
    if (logging_service_) {
        logging_service_->LogDebug("CMPService", message);
    }
}

}  // namespace DomainServices
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen
