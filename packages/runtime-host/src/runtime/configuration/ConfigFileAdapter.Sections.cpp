// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ConfigFile"

#include "ConfigFileAdapter.h"
#include "shared/interfaces/ILoggingService.h"
#include <algorithm>

namespace Siligen::Infrastructure::Adapters {

using namespace Domain::Configuration::Ports;
using namespace Shared::Types;

// === 配置段读取 ===

Result<void> ConfigFileAdapter::LoadDispensingSection(DispensingConfig& config) {
    const std::string section = "Dispensing";
    auto assign_float = [this](const std::string& sec, const std::string& key, float32& out) -> Result<void> {
        auto result = ReadFloatValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_bool = [this](const std::string& sec, const std::string& key, bool& out) -> Result<void> {
        auto result = ReadBoolValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_int = [this](const std::string& sec, const std::string& key, int& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto result = assign_float(section, "pressure", config.pressure);
    if (result.IsError()) return result;
    result = assign_float(section, "flow_rate", config.flow_rate);
    if (result.IsError()) return result;
    result = assign_float(section, "dispensing_time", config.dispensing_time);
    if (result.IsError()) return result;
    result = assign_float(section, "wait_time", config.wait_time);
    if (result.IsError()) return result;
    auto stabilization_result = assign_int(section, "supply_stabilization_ms", config.supply_stabilization_ms);
    if (stabilization_result.IsError()) {
        config.supply_stabilization_ms = 500;
    }
    result = assign_float(section, "retract_height", config.retract_height);
    if (result.IsError()) return result;
    result = assign_float(section, "approach_height", config.approach_height);
    if (result.IsError()) return result;

    auto mode_result = ReadDispensingModeValue(section, "mode");
    if (mode_result.IsError()) {
        return Result<void>(mode_result.GetError());
    }
    config.mode = mode_result.Value();

    // 读取点胶策略配置
    auto strategy_str_result = ReadIniValue(section, "strategy");
    if (strategy_str_result.IsSuccess()) {
        std::string strategy_str = strategy_str_result.Value();
        if (strategy_str == "segmented") {
            config.strategy = DispensingStrategy::SEGMENTED;
        } else if (strategy_str == "subsegmented") {
            config.strategy = DispensingStrategy::SUBSEGMENTED;
        } else if (strategy_str == "cruise_only") {
            config.strategy = DispensingStrategy::CRUISE_ONLY;
        } else {
            config.strategy = DispensingStrategy::BASELINE;
        }
    } else {
        config.strategy = DispensingStrategy::BASELINE;
    }

    auto subsegment_result = assign_int(section, "subsegment_count", config.subsegment_count);
    if (subsegment_result.IsError()) {
        config.subsegment_count = 8; // Default
    }
    auto cruise_only_result = assign_bool(section, "dispense_only_cruise", config.dispense_only_cruise);
    if (cruise_only_result.IsError()) {
        config.dispense_only_cruise = false;
    }
    result = assign_float(section, "open_comp_ms", config.open_comp_ms);
    if (result.IsError()) return result;
    result = assign_float(section, "close_comp_ms", config.close_comp_ms);
    if (result.IsError()) return result;
    result = assign_bool(section, "retract_enabled", config.retract_enabled);
    if (result.IsError()) return result;
    result = assign_float(section, "corner_pulse_scale", config.corner_pulse_scale);
    if (result.IsError()) return result;
    result = assign_float(section, "curvature_speed_factor", config.curvature_speed_factor);
    if (result.IsError()) return result;
    result = assign_float(section, "start_speed_factor", config.start_speed_factor);
    if (result.IsError()) {
        config.start_speed_factor = 0.5f;
    }
    result = assign_float(section, "end_speed_factor", config.end_speed_factor);
    if (result.IsError()) {
        config.end_speed_factor = 0.5f;
    }
    result = assign_float(section, "corner_speed_factor", config.corner_speed_factor);
    if (result.IsError()) {
        config.corner_speed_factor = 0.6f;
    }
    result = assign_float(section, "rapid_speed_factor", config.rapid_speed_factor);
    if (result.IsError()) {
        config.rapid_speed_factor = 1.0f;
    }
    result = assign_float(section, "trajectory_sample_dt", config.trajectory_sample_dt);
    if (result.IsError()) {
        config.trajectory_sample_dt = 0.01f;
    }
    result = assign_float(section, "trajectory_sample_ds", config.trajectory_sample_ds);
    if (result.IsError()) {
        config.trajectory_sample_ds = 0.0f;
    }

    result = assign_float(section, "temperature_target_c", config.temperature_target_c);
    if (result.IsError()) return result;
    result = assign_float(section, "temperature_tolerance_c", config.temperature_tolerance_c);
    if (result.IsError()) return result;
    result = assign_float(section, "viscosity_target_cps", config.viscosity_target_cps);
    if (result.IsError()) return result;
    result = assign_float(section, "viscosity_tolerance_pct", config.viscosity_tolerance_pct);
    if (result.IsError()) return result;
    result = assign_bool(section, "air_dry_required", config.air_dry_required);
    if (result.IsError()) return result;
    result = assign_bool(section, "air_oil_free_required", config.air_oil_free_required);
    if (result.IsError()) return result;
    result = assign_bool(section, "suck_back_enabled", config.suck_back_enabled);
    if (result.IsError()) return result;
    result = assign_float(section, "suck_back_ms", config.suck_back_ms);
    if (result.IsError()) return result;
    result = assign_bool(section, "vision_feedback_enabled", config.vision_feedback_enabled);
    if (result.IsError()) return result;
    result = assign_float(section, "dot_diameter_target_mm", config.dot_diameter_target_mm);
    if (result.IsError()) return result;
    result = assign_float(section, "dot_edge_gap_mm", config.dot_edge_gap_mm);
    if (result.IsError()) return result;
    result = assign_float(section, "dot_diameter_tolerance_mm", config.dot_diameter_tolerance_mm);
    if (result.IsError()) return result;
    result = assign_float(section, "syringe_level_min_pct", config.syringe_level_min_pct);
    if (result.IsError()) return result;

    return Result<void>();
}

Result<void> ConfigFileAdapter::LoadMachineSection(MachineConfig& config) {
    const std::string section = "Machine";
    auto assign_float = [this](const std::string& sec, const std::string& key, float32& out) -> Result<void> {
        auto result = ReadFloatValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto result = assign_float(section, "work_area_width", config.work_area_width);
    if (result.IsError()) return result;
    result = assign_float(section, "work_area_height", config.work_area_height);
    if (result.IsError()) return result;
    result = assign_float(section, "z_axis_range", config.z_axis_range);
    if (result.IsError()) return result;
    result = assign_float(section, "max_speed", config.max_speed);
    if (result.IsError()) return result;
    result = assign_float(section, "max_acceleration", config.max_acceleration);
    if (result.IsError()) return result;
    result = assign_float(section, "positioning_tolerance", config.positioning_tolerance);
    if (result.IsError()) return result;
    result = assign_float(section, "pulse_per_mm", config.pulse_per_mm);
    if (result.IsError()) return result;

    result = assign_float(section, "x_min", config.soft_limits.x_min);
    if (result.IsError()) return result;
    result = assign_float(section, "x_max", config.soft_limits.x_max);
    if (result.IsError()) return result;
    result = assign_float(section, "y_min", config.soft_limits.y_min);
    if (result.IsError()) return result;
    result = assign_float(section, "y_max", config.soft_limits.y_max);
    if (result.IsError()) return result;
    result = assign_float(section, "z_min", config.soft_limits.z_min);
    if (result.IsError()) return result;
    result = assign_float(section, "z_max", config.soft_limits.z_max);
    if (result.IsError()) return result;

    return Result<void>();
}

Result<void> ConfigFileAdapter::LoadDxfSection(DxfConfig& config) {
    const std::string section = "DXF";

    auto offset_x_result = ReadFloatValue(section, "offset_x");
    if (offset_x_result.IsSuccess()) {
        config.offset_x = offset_x_result.Value();
    } else {
        config.offset_x = 0.0f;
    }

    auto offset_y_result = ReadFloatValue(section, "offset_y");
    if (offset_y_result.IsSuccess()) {
        config.offset_y = offset_y_result.Value();
    } else {
        config.offset_y = 0.0f;
    }

    return Result<void>();
}

Result<void> ConfigFileAdapter::LoadHomingSection(std::vector<HomingConfig>& configs) {
    configs.clear();

    auto assign_float = [this](const std::string& sec, const std::string& key, float32& out) -> Result<void> {
        auto result = ReadFloatValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_int = [this](const std::string& sec, const std::string& key, int32& out) -> Result<void> {
        auto result = ReadIntValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };
    auto assign_bool = [this](const std::string& sec, const std::string& key, bool& out) -> Result<void> {
        auto result = ReadBoolValue(sec, key);
        if (result.IsError()) {
            return Result<void>(result.GetError());
        }
        out = result.Value();
        return Result<void>();
    };

    auto axis_count_result = ReadIntValue("Hardware", "num_axes");
    if (axis_count_result.IsError()) {
        return Result<void>(axis_count_result.GetError());
    }
    const int axis_count = static_cast<int>(Shared::Types::HardwareConfiguration::ClampAxisCount(
        axis_count_result.Value()));

    for (int axis = 0; axis < axis_count; ++axis) {
        HomingConfig config;
        std::string section = "Homing_Axis" + std::to_string(axis + 1);

        config.axis = axis;

        auto result = assign_int(section, "mode", config.mode);
        if (result.IsError()) return result;
        result = assign_int(section, "direction", config.direction);
        if (result.IsError()) return result;

        result = assign_int(section, "home_input_source", config.home_input_source);
        if (result.IsError()) return result;
        result = assign_int(section, "home_input_bit", config.home_input_bit);
        if (result.IsError()) return result;
        result = assign_bool(section, "home_active_low", config.home_active_low);
        if (result.IsError()) return result;
        result = assign_int(section, "home_debounce_ms", config.home_debounce_ms);
        if (result.IsError()) return result;

        result = assign_float(section, "rapid_velocity", config.rapid_velocity);
        if (result.IsError()) return result;
        result = assign_float(section, "locate_velocity", config.locate_velocity);
        if (result.IsError()) return result;
        result = assign_float(section, "index_velocity", config.index_velocity);
        if (result.IsError()) return result;

        result = assign_float(section, "acceleration", config.acceleration);
        if (result.IsError()) return result;
        result = assign_float(section, "deceleration", config.deceleration);
        if (result.IsError()) return result;
        result = assign_float(section, "jerk", config.jerk);
        if (result.IsError()) return result;

        result = assign_float(section, "offset", config.offset);
        if (result.IsError()) return result;
        result = assign_float(section, "search_distance", config.search_distance);
        if (result.IsError()) return result;
        result = assign_float(section, "escape_distance", config.escape_distance);
        if (result.IsError()) return result;
        result = assign_float(section, "escape_velocity", config.escape_velocity);
        if (result.IsError()) return result;
        auto home_backoff_result = ReadBoolValue(section, "home_backoff_enabled");
        if (home_backoff_result.IsSuccess()) {
            config.home_backoff_enabled = home_backoff_result.Value();
        } else {
            const auto& message = home_backoff_result.GetError().GetMessage();
            if (message.find("缺少配置项") != std::string::npos) {
                SILIGEN_LOG_WARNING(
                    "Missing [" + section + "] home_backoff_enabled, keeping compatibility default=true");
            } else {
                return Result<void>(home_backoff_result.GetError());
            }
        }

        result = assign_int(section, "timeout_ms", config.timeout_ms);
        if (result.IsError()) return result;
        result = assign_int(section, "settle_time_ms", config.settle_time_ms);
        if (result.IsError()) return result;
        result = assign_int(section, "escape_timeout_ms", config.escape_timeout_ms);
        if (result.IsError()) return result;

        result = assign_int(section, "retry_count", config.retry_count);
        if (result.IsError()) return result;
        result = assign_bool(section, "enable_escape", config.enable_escape);
        if (result.IsError()) return result;
        result = assign_bool(section, "enable_limit_switch", config.enable_limit_switch);
        if (result.IsError()) return result;
        result = assign_bool(section, "enable_index", config.enable_index);
        if (result.IsError()) return result;
        result = assign_int(section, "escape_max_attempts", config.escape_max_attempts);
        if (result.IsError()) return result;

        configs.push_back(config);
    }

    return Result<void>();
}

// === 配置段写入 ===

void ConfigFileAdapter::SaveDispensingSection(const DispensingConfig& config) const {
    WriteIniValue("Dispensing", "pressure", FloatToString(config.pressure));
    WriteIniValue("Dispensing", "flow_rate", FloatToString(config.flow_rate));
    WriteIniValue("Dispensing", "dispensing_time", FloatToString(config.dispensing_time));
    WriteIniValue("Dispensing", "wait_time", FloatToString(config.wait_time));
    WriteIniValue("Dispensing", "supply_stabilization_ms", IntToString(config.supply_stabilization_ms));
    WriteIniValue("Dispensing", "retract_height", FloatToString(config.retract_height));
    WriteIniValue("Dispensing", "approach_height", FloatToString(config.approach_height));

    WriteIniValue("Dispensing", "mode", Siligen::ToString(config.mode));

    std::string strategy_str;
    switch (config.strategy) {
        case DispensingStrategy::SEGMENTED:
            strategy_str = "segmented";
            break;
        case DispensingStrategy::SUBSEGMENTED:
            strategy_str = "subsegmented";
            break;
        case DispensingStrategy::CRUISE_ONLY:
            strategy_str = "cruise_only";
            break;
        case DispensingStrategy::BASELINE:
        default:
            strategy_str = "baseline";
            break;
    }
    WriteIniValue("Dispensing", "strategy", strategy_str);
    WriteIniValue("Dispensing", "subsegment_count", IntToString(config.subsegment_count));
    WriteIniValue("Dispensing", "dispense_only_cruise", BoolToString(config.dispense_only_cruise));
    WriteIniValue("Dispensing", "open_comp_ms", FloatToString(config.open_comp_ms));
    WriteIniValue("Dispensing", "close_comp_ms", FloatToString(config.close_comp_ms));
    WriteIniValue("Dispensing", "retract_enabled", BoolToString(config.retract_enabled));
    WriteIniValue("Dispensing", "corner_pulse_scale", FloatToString(config.corner_pulse_scale));
    WriteIniValue("Dispensing", "curvature_speed_factor", FloatToString(config.curvature_speed_factor));
    WriteIniValue("Dispensing", "start_speed_factor", FloatToString(config.start_speed_factor));
    WriteIniValue("Dispensing", "end_speed_factor", FloatToString(config.end_speed_factor));
    WriteIniValue("Dispensing", "corner_speed_factor", FloatToString(config.corner_speed_factor));
    WriteIniValue("Dispensing", "rapid_speed_factor", FloatToString(config.rapid_speed_factor));
    WriteIniValue("Dispensing", "trajectory_sample_dt", FloatToString(config.trajectory_sample_dt));
    WriteIniValue("Dispensing", "trajectory_sample_ds", FloatToString(config.trajectory_sample_ds));

    WriteIniValue("Dispensing", "temperature_target_c", FloatToString(config.temperature_target_c));
    WriteIniValue("Dispensing", "temperature_tolerance_c", FloatToString(config.temperature_tolerance_c));
    WriteIniValue("Dispensing", "viscosity_target_cps", FloatToString(config.viscosity_target_cps));
    WriteIniValue("Dispensing", "viscosity_tolerance_pct", FloatToString(config.viscosity_tolerance_pct));
    WriteIniValue("Dispensing", "air_dry_required", BoolToString(config.air_dry_required));
    WriteIniValue("Dispensing", "air_oil_free_required", BoolToString(config.air_oil_free_required));
    WriteIniValue("Dispensing", "suck_back_enabled", BoolToString(config.suck_back_enabled));
    WriteIniValue("Dispensing", "suck_back_ms", FloatToString(config.suck_back_ms));
    WriteIniValue("Dispensing", "vision_feedback_enabled", BoolToString(config.vision_feedback_enabled));
    WriteIniValue("Dispensing", "dot_diameter_target_mm", FloatToString(config.dot_diameter_target_mm));
    WriteIniValue("Dispensing", "dot_edge_gap_mm", FloatToString(config.dot_edge_gap_mm));
    WriteIniValue("Dispensing", "dot_diameter_tolerance_mm", FloatToString(config.dot_diameter_tolerance_mm));
    WriteIniValue("Dispensing", "syringe_level_min_pct", FloatToString(config.syringe_level_min_pct));
}

void ConfigFileAdapter::SaveMachineSection(const MachineConfig& config) const {
    WriteIniValue("Machine", "work_area_width", FloatToString(config.work_area_width));
    WriteIniValue("Machine", "work_area_height", FloatToString(config.work_area_height));
    WriteIniValue("Machine", "z_axis_range", FloatToString(config.z_axis_range));
    WriteIniValue("Machine", "max_speed", FloatToString(config.max_speed));
    WriteIniValue("Machine", "max_acceleration", FloatToString(config.max_acceleration));
    WriteIniValue("Machine", "positioning_tolerance", FloatToString(config.positioning_tolerance));
    WriteIniValue("Machine", "pulse_per_mm", FloatToString(config.pulse_per_mm));

    WriteIniValue("Machine", "x_min", FloatToString(config.soft_limits.x_min));
    WriteIniValue("Machine", "x_max", FloatToString(config.soft_limits.x_max));
    WriteIniValue("Machine", "y_min", FloatToString(config.soft_limits.y_min));
    WriteIniValue("Machine", "y_max", FloatToString(config.soft_limits.y_max));
    WriteIniValue("Machine", "z_min", FloatToString(config.soft_limits.z_min));
    WriteIniValue("Machine", "z_max", FloatToString(config.soft_limits.z_max));
}

void ConfigFileAdapter::SaveDxfSection(const DxfConfig& config) const {
    const std::string section = "DXF";
    WriteIniValue(section, "offset_x", FloatToString(config.offset_x));
    WriteIniValue(section, "offset_y", FloatToString(config.offset_y));
}

void ConfigFileAdapter::SaveHomingSection(const std::vector<HomingConfig>& configs) const {
    for (const auto& config : configs) {
        std::string section = "Homing_Axis" + std::to_string(config.axis + 1);

        // 基本信息
        WriteIniValue(section, "mode", IntToString(config.mode));
        WriteIniValue(section, "direction", IntToString(config.direction));

        // HOME输入参数
        WriteIniValue(section, "home_input_source", IntToString(config.home_input_source));
        WriteIniValue(section, "home_input_bit", IntToString(config.home_input_bit));
        WriteIniValue(section, "home_active_low", BoolToString(config.home_active_low));
        WriteIniValue(section, "home_debounce_ms", IntToString(config.home_debounce_ms));

        // 速度参数
        WriteIniValue(section, "rapid_velocity", FloatToString(config.rapid_velocity));
        WriteIniValue(section, "locate_velocity", FloatToString(config.locate_velocity));
        WriteIniValue(section, "index_velocity", FloatToString(config.index_velocity));

        // 加速度参数
        WriteIniValue(section, "acceleration", FloatToString(config.acceleration));
        WriteIniValue(section, "deceleration", FloatToString(config.deceleration));
        WriteIniValue(section, "jerk", FloatToString(config.jerk));

        // 距离参数
        WriteIniValue(section, "offset", FloatToString(config.offset));
        WriteIniValue(section, "search_distance", FloatToString(config.search_distance));
        WriteIniValue(section, "escape_distance", FloatToString(config.escape_distance));
        WriteIniValue(section, "escape_velocity", FloatToString(config.escape_velocity));
        WriteIniValue(section, "home_backoff_enabled", BoolToString(config.home_backoff_enabled));

        // 时间参数
        WriteIniValue(section, "timeout_ms", IntToString(config.timeout_ms));
        WriteIniValue(section, "settle_time_ms", IntToString(config.settle_time_ms));
        WriteIniValue(section, "escape_timeout_ms", IntToString(config.escape_timeout_ms));

        // 其他参数
        WriteIniValue(section, "retry_count", IntToString(config.retry_count));
        WriteIniValue(section, "enable_escape", BoolToString(config.enable_escape));
        WriteIniValue(section, "enable_limit_switch", BoolToString(config.enable_limit_switch));
        WriteIniValue(section, "enable_index", BoolToString(config.enable_index));
        WriteIniValue(section, "escape_max_attempts", IntToString(config.escape_max_attempts));
    }
}

// === 配置验证辅助方法 ===

bool ConfigFileAdapter::ValidateDispensingConfig(const DispensingConfig& config,
                                                 std::vector<std::string>& errors) const {
    bool valid = true;

    if (config.pressure <= 0 || config.pressure > 1000) {
        errors.push_back("点胶压力超出有效范围 (0-1000 kPa)");
        valid = false;
    }

    if (config.flow_rate <= 0 || config.flow_rate > 100) {
        errors.push_back("流量超出有效范围 (0-100 ml/min)");
        valid = false;
    }

    if (config.dispensing_time <= 0 || config.dispensing_time > 60) {
        errors.push_back("点胶时间超出有效范围 (0-60 s)");
        valid = false;
    }

    if (config.supply_stabilization_ms < 0 || config.supply_stabilization_ms > 5000) {
        errors.push_back("供胶阀稳压时间超出有效范围 (0-5000 ms)");
        valid = false;
    }

    if (config.temperature_target_c < 0.0f || config.temperature_target_c > 200.0f) {
        errors.push_back("目标温度超出有效范围 (0-200 C, 0=不启用)");
        valid = false;
    }
    if (config.temperature_tolerance_c < 0.0f || config.temperature_tolerance_c > 50.0f) {
        errors.push_back("温度容差超出有效范围 (0-50 C)");
        valid = false;
    }
    if (config.temperature_target_c > 0.0f && config.temperature_tolerance_c <= 0.0f) {
        errors.push_back("设置目标温度时必须配置正的温度容差");
        valid = false;
    }

    if (config.viscosity_target_cps < 0.0f || config.viscosity_target_cps > 5000000.0f) {
        errors.push_back("目标黏度超出有效范围 (0-5000000 cP, 0=不启用)");
        valid = false;
    }
    if (config.viscosity_tolerance_pct < 0.0f || config.viscosity_tolerance_pct > 100.0f) {
        errors.push_back("黏度容差超出有效范围 (0-100%)");
        valid = false;
    }
    if (config.viscosity_target_cps > 0.0f && config.viscosity_tolerance_pct <= 0.0f) {
        errors.push_back("设置目标黏度时必须配置正的黏度容差");
        valid = false;
    }

    if (config.suck_back_ms < 0.0f || config.suck_back_ms > 5000.0f) {
        errors.push_back("回吸时间超出有效范围 (0-5000 ms)");
        valid = false;
    }
    if (config.suck_back_enabled && config.suck_back_ms <= 0.0f) {
        errors.push_back("启用回吸时必须设置回吸时间");
        valid = false;
    }

    if (config.subsegment_count <= 0 || config.subsegment_count > 64) {
        errors.push_back("子段数量超出有效范围 (1-64)");
        valid = false;
    }
    if (config.open_comp_ms < 0.0f || config.open_comp_ms > 5000.0f) {
        errors.push_back("起胶补偿超出有效范围 (0-5000 ms)");
        valid = false;
    }
    if (config.close_comp_ms < 0.0f || config.close_comp_ms > 5000.0f) {
        errors.push_back("收胶补偿超出有效范围 (0-5000 ms)");
        valid = false;
    }
    if (config.corner_pulse_scale < 0.1f || config.corner_pulse_scale > 2.0f) {
        errors.push_back("拐角胶量缩放系数超出有效范围 (0.1-2.0)");
        valid = false;
    }
    if (config.curvature_speed_factor < 0.1f || config.curvature_speed_factor > 1.0f) {
        errors.push_back("曲率限速系数超出有效范围 (0.1-1.0)");
        valid = false;
    }
    if (config.start_speed_factor < 0.0f || config.start_speed_factor > 1.0f) {
        errors.push_back("起始速度系数超出有效范围 (0.0-1.0)");
        valid = false;
    }
    if (config.end_speed_factor < 0.0f || config.end_speed_factor > 1.0f) {
        errors.push_back("结束速度系数超出有效范围 (0.0-1.0)");
        valid = false;
    }
    if (config.corner_speed_factor < 0.0f || config.corner_speed_factor > 1.0f) {
        errors.push_back("拐角速度系数超出有效范围 (0.0-1.0)");
        valid = false;
    }
    if (config.rapid_speed_factor < 0.0f || config.rapid_speed_factor > 1.0f) {
        errors.push_back("快速段速度系数超出有效范围 (0.0-1.0)");
        valid = false;
    }
    if (config.trajectory_sample_dt < 0.0f || config.trajectory_sample_dt > 0.1f) {
        errors.push_back("轨迹采样时间步长超出有效范围 (0.0-0.1 s, 0=自动)");
        valid = false;
    }
    if (config.trajectory_sample_ds < 0.0f || config.trajectory_sample_ds > 5.0f) {
        errors.push_back("轨迹采样弧长步长超出有效范围 (0.0-5.0 mm, 0=自动)");
        valid = false;
    }

    if (config.dot_diameter_target_mm < 0.0f || config.dot_diameter_target_mm > 100.0f) {
        errors.push_back("点径目标超出有效范围 (0-100 mm, 0=不启用)");
        valid = false;
    }
    if (config.dot_edge_gap_mm < 0.0f || config.dot_edge_gap_mm > 100.0f) {
        errors.push_back("点间隙超出有效范围 (0-100 mm)");
        valid = false;
    }
    if (config.dot_edge_gap_mm > 0.0f && config.dot_diameter_target_mm <= 0.0f) {
        errors.push_back("设置点间隙时必须设置正的点径目标");
        valid = false;
    }
    if (config.dot_diameter_tolerance_mm < 0.0f || config.dot_diameter_tolerance_mm > 100.0f) {
        errors.push_back("点径容差超出有效范围 (0-100 mm)");
        valid = false;
    }
    if (config.vision_feedback_enabled &&
        (config.dot_diameter_target_mm <= 0.0f || config.dot_diameter_tolerance_mm <= 0.0f)) {
        errors.push_back("启用点径反馈时必须设置目标点径和容差");
        valid = false;
    }

    if (config.syringe_level_min_pct < 0.0f || config.syringe_level_min_pct > 100.0f) {
        errors.push_back("料筒液位下限超出有效范围 (0-100%)");
        valid = false;
    }

    return valid;
}

bool ConfigFileAdapter::ValidateMachineConfig(const MachineConfig& config, std::vector<std::string>& errors) const {
    bool valid = true;

    if (config.max_speed <= 0 || config.max_speed > 1000) {
        errors.push_back("最大速度超出有效范围 (0-1000 mm/s)");
        valid = false;
    }

    if (config.max_acceleration <= 0 || config.max_acceleration > 10000) {
        errors.push_back("最大加速度超出有效范围 (0-10000 mm/s²)");
        valid = false;
    }

    return valid;
}

}  // namespace Siligen::Infrastructure::Adapters

