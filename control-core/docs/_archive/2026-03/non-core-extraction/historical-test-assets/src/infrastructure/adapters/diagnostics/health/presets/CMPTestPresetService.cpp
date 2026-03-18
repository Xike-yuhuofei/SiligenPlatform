// CMPTestPresetService.cpp
// 版本: 1.0.0
// 描述: CMP测试预设服务实现

#include "CMPTestPresetService.h"

namespace Siligen {
namespace Infrastructure {
namespace Services {

CMPTestPresetService::CMPTestPresetService() : initialized_(false) {
    initializePresets();
}

Result<std::vector<CMPTestPreset>> CMPTestPresetService::loadAllPresets() {
    if (!initialized_) {
        Error error(ErrorCode::CONFIGURATION_ERROR, "预设未初始化", "CMPTestPresetService");
        return Result<std::vector<CMPTestPreset>>::Failure(error);
    }

    std::vector<CMPTestPreset> result;
    result.reserve(presets_.size());

    for (const auto& pair : presets_) {
        result.push_back(pair.second);
    }

    return Result<std::vector<CMPTestPreset>>::Success(result);
}

Result<CMPTestPreset> CMPTestPresetService::getPresetByName(const std::string& name) {
    auto it = presets_.find(name);
    if (it == presets_.end()) {
        Error error(ErrorCode::INVALID_PARAMETER, "未找到预设: " + name, "CMPTestPresetService");
        return Result<CMPTestPreset>::Failure(error);
    }

    return Result<CMPTestPreset>::Success(it->second);
}

std::vector<CMPTestPreset> CMPTestPresetService::listPresetsByCategory(PresetCategory category) {
    std::vector<CMPTestPreset> result;

    for (const auto& pair : presets_) {
        if (pair.second.category == category) {
            result.push_back(pair.second);
        }
    }

    return result;
}

Result<void> CMPTestPresetService::validatePreset(const CMPTestPreset& preset) {
    if (!preset.Validate()) {
        Error error(ErrorCode::ValidationFailed, "预设验证失败: " + preset.name, "CMPTestPresetService");
        return Result<void>::Failure(error);
    }

    return Result<void>::Success();
}

size_t CMPTestPresetService::getPresetCount() const {
    return presets_.size();
}

void CMPTestPresetService::initializePresets() {
    presets_.clear();

    // 创建3个基础预设
    auto preset1 = createSimpleLinearPreset();
    auto preset2 = createCircularArcPreset();
    auto preset3 = createHighSpeedPreset();

    // 添加到存储
    presets_[preset1.name] = preset1;
    presets_[preset2.name] = preset2;
    presets_[preset3.name] = preset3;

    initialized_ = true;
}

CMPTestPreset CMPTestPresetService::createSimpleLinearPreset() {
    CMPTestPreset preset;

    preset.name = "简单直线点胶";
    preset.description = "基础验证场景 - 单轴低速直线运动，位置同步触发";
    preset.category = PresetCategory::Basic;

    // 轨迹参数
    preset.params.trajectory.startPosition = {0.0f, 0.0f};
    preset.params.trajectory.endPosition = {100.0f, 0.0f};
    preset.params.trajectory.velocity = 50.0f;
    preset.params.trajectory.acceleration = 100.0f;
    preset.params.trajectory.timeStep = 0.01f;

    // CMP配置
    preset.params.cmpConfig.trigger_mode = CMPTriggerMode::POSITION_SYNC;
    preset.params.cmpConfig.is_enabled = true;
    preset.params.cmpConfig.start_position = 0;
    preset.params.cmpConfig.end_position = 100000;  // 100mm * 1000 脉冲/mm
    preset.params.cmpConfig.enable_compensation = false;
    preset.params.cmpConfig.compensation_factor = 1.0f;
    preset.params.cmpConfig.trigger_position_tolerance = 0.1f;
    preset.params.cmpConfig.time_tolerance_ms = 1.0f;
    preset.params.cmpConfig.enable_multi_channel = false;
    preset.params.cmpConfig.cmp_channel = 1;
    preset.params.cmpConfig.pulse_width_us = 2000;

    // 触发点: 每10mm一个触发点 (9个点)
    preset.params.cmpConfig.trigger_points.clear();
    for (int i = 1; i <= 9; ++i) {
        CMPTriggerPoint point;
        point.position = i * 10000;  // 10mm, 20mm, ..., 90mm
        point.action = DispensingAction::PULSE;
        point.pulse_width_us = 2000;
        point.delay_time_us = 0;
        point.is_enabled = true;
        preset.params.cmpConfig.trigger_points.push_back(point);
    }

    // 采样和容差
    preset.params.samplingRateHz = 100.0f;
    preset.params.validationTolerance.positionToleranceMM = 0.1f;
    preset.params.validationTolerance.timeToleranceMS = 1.0f;
    preset.params.validationTolerance.velocityTolerancePercent = 5.0f;

    // 预期结果
    preset.expectedResults.maxDeviationMM = 0.1f;
    preset.expectedResults.successRate = 95.0f;

    return preset;
}

CMPTestPreset CMPTestPresetService::createCircularArcPreset() {
    CMPTestPreset preset;

    preset.name = "圆弧轨迹点胶";
    preset.description = "进阶场景 - 双轴协调插补，序列触发模式";
    preset.category = PresetCategory::Advanced;

    // 轨迹参数 (对角线运动, 距离约70.7mm)
    preset.params.trajectory.startPosition = {0.0f, 0.0f};
    preset.params.trajectory.endPosition = {50.0f, 50.0f};
    preset.params.trajectory.velocity = 100.0f;
    preset.params.trajectory.acceleration = 200.0f;
    preset.params.trajectory.timeStep = 0.01f;

    // CMP配置
    preset.params.cmpConfig.trigger_mode = CMPTriggerMode::SEQUENCE;
    preset.params.cmpConfig.is_enabled = true;
    preset.params.cmpConfig.start_position = 0;
    preset.params.cmpConfig.end_position = 70711;  // sqrt(50^2 + 50^2) * 1000
    preset.params.cmpConfig.enable_compensation = true;
    preset.params.cmpConfig.compensation_factor = 1.5f;
    preset.params.cmpConfig.trigger_position_tolerance = 0.1f;
    preset.params.cmpConfig.time_tolerance_ms = 1.0f;
    preset.params.cmpConfig.enable_multi_channel = false;
    preset.params.cmpConfig.cmp_channel = 1;
    preset.params.cmpConfig.pulse_width_us = 2000;

    // 触发点: 7个触发点，每10mm一个
    preset.params.cmpConfig.trigger_points.clear();
    int positions[] = {5000, 15000, 25000, 35000, 45000, 55000, 65000};
    for (int pos : positions) {
        CMPTriggerPoint point;
        point.position = pos;
        point.action = DispensingAction::PULSE;
        point.pulse_width_us = 2000;
        point.delay_time_us = 0;
        point.is_enabled = true;
        preset.params.cmpConfig.trigger_points.push_back(point);
    }

    // 采样和容差
    preset.params.samplingRateHz = 100.0f;
    preset.params.validationTolerance.positionToleranceMM = 0.05f;
    preset.params.validationTolerance.timeToleranceMS = 1.0f;
    preset.params.validationTolerance.velocityTolerancePercent = 5.0f;

    // 预期结果
    preset.expectedResults.maxDeviationMM = 0.05f;
    preset.expectedResults.successRate = 98.0f;

    return preset;
}

CMPTestPreset CMPTestPresetService::createHighSpeedPreset() {
    CMPTestPreset preset;

    preset.name = "高速精度测试";
    preset.description = "性能基准 - 极限速度容差验证，范围触发模式";
    preset.category = PresetCategory::Performance;

    // 轨迹参数
    preset.params.trajectory.startPosition = {0.0f, 0.0f};
    preset.params.trajectory.endPosition = {200.0f, 0.0f};
    preset.params.trajectory.velocity = 300.0f;
    preset.params.trajectory.acceleration = 1000.0f;
    preset.params.trajectory.timeStep = 0.01f;

    // CMP配置
    preset.params.cmpConfig.trigger_mode = CMPTriggerMode::RANGE;
    preset.params.cmpConfig.is_enabled = true;
    preset.params.cmpConfig.start_position = 20000;  // 从20mm开始
    preset.params.cmpConfig.end_position = 180000;   // 到180mm结束
    preset.params.cmpConfig.enable_compensation = false;
    preset.params.cmpConfig.compensation_factor = 1.0f;
    preset.params.cmpConfig.trigger_position_tolerance = 0.01f;
    preset.params.cmpConfig.time_tolerance_ms = 0.5f;
    preset.params.cmpConfig.enable_multi_channel = false;
    preset.params.cmpConfig.cmp_channel = 1;
    preset.params.cmpConfig.pulse_width_us = 1000;  // 更短的脉冲宽度

    // RANGE模式不需要trigger_points
    preset.params.cmpConfig.trigger_points.clear();

    // 采样和容差
    preset.params.samplingRateHz = 200.0f;  // 更高的采样率
    preset.params.validationTolerance.positionToleranceMM = 0.01f;
    preset.params.validationTolerance.timeToleranceMS = 0.5f;
    preset.params.validationTolerance.velocityTolerancePercent = 3.0f;

    // 预期结果
    preset.expectedResults.maxDeviationMM = 0.02f;
    preset.expectedResults.successRate = 99.0f;

    return preset;
}

}  // namespace Services
}  // namespace Infrastructure
}  // namespace Siligen
