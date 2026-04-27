// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ConfigValidator"

#include "ConfigValidator.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include "shared/interfaces/ILoggingService.h"

namespace Siligen {

// === 回零配置验证实现 ===

bool ConfigValidator::ValidateHomingConfig(const HomingConfig& config) {
    ValidationResult result = ValidateHomingConfigDetailed(config);

    // 输出错误信息
    for (const auto& error : result.errors) {
        SILIGEN_LOG_ERROR(error);
    }

    // 输出警告信息
    for (const auto& warning : result.warnings) {
        SILIGEN_LOG_WARNING(warning);
    }

    return result.is_valid;
}

ValidationResult ConfigValidator::ValidateHomingConfigDetailed(const HomingConfig& config) {
    ValidationResult result;
    constexpr float32 kHomingMaxVelocity = 10.0f;

    // 验证基本参数
    if (!ValidateRange(config.mode, 0, 9)) {
        result.AddError(FormatRangeError("回零模式", config.mode, 0, 9));
    }

    if (!ValidateRange(config.direction, 0, 1)) {
        result.AddError(FormatRangeError("回零方向", config.direction, 0, 1));
    }

    if (!ValidateRange(config.home_input_source, 0, 7)) {
        result.AddError(FormatRangeError("HOME输入源", config.home_input_source, 0, 7));
    }

    if (config.home_input_bit < -1 || config.home_input_bit > 15) {
        result.AddError(FormatRangeError("HOME输入位", config.home_input_bit, -1, 15));
    }

    if (config.home_debounce_ms < 0 || config.home_debounce_ms > 1000) {
        result.AddError(FormatRangeError("HOME去抖时间", config.home_debounce_ms, 0, 1000));
    }

    if (!ValidateTimeout(config.timeout_ms)) {
        result.AddError(FormatRangeError("超时时间", config.timeout_ms, 100, 300000));
    }

    const bool has_ready_zero_speed = config.ready_zero_speed_mm_s > 0.0f;

    if (has_ready_zero_speed) {
        if (!ValidateVelocity(config.ready_zero_speed_mm_s, 0.1f, kHomingMaxVelocity)) {
            result.AddError(FormatRangeError("统一回零速度", config.ready_zero_speed_mm_s, 0.1f, kHomingMaxVelocity));
        }
    } else {
        result.AddError("必须显式配置正的 ready_zero_speed_mm_s");
    }

    // 验证速度参数
    if (!ValidateVelocity(config.rapid_velocity, 0.1f, kHomingMaxVelocity)) {
        result.AddError(FormatRangeError("快速回零速度", config.rapid_velocity, 0.1f, kHomingMaxVelocity));
    }

    if (!ValidateVelocity(config.locate_velocity, 0.1f, kHomingMaxVelocity)) {
        result.AddError(FormatRangeError("定位速度", config.locate_velocity, 0.1f, kHomingMaxVelocity));
    }

    if (!ValidateVelocity(config.index_velocity, 0.1f, kHomingMaxVelocity)) {
        result.AddError(FormatRangeError("索引速度", config.index_velocity, 0.1f, kHomingMaxVelocity));
    }

    // 验证速度关系
    if (!has_ready_zero_speed &&
        !ValidateVelocityRelationship(config.rapid_velocity, config.locate_velocity, config.index_velocity)) {
        result.AddError("速度参数关系错误: 快速速度 > 定位速度 > 索引速度");
    }

    // 验证加速度参数
    if (!ValidateAcceleration(config.acceleration)) {
        result.AddError(FormatRangeError("加速度", config.acceleration, 1.0f, 10000.0f));
    }

    if (!ValidateAcceleration(config.deceleration)) {
        result.AddError(FormatRangeError("减速度", config.deceleration, 1.0f, 10000.0f));
    }

    if (config.jerk < 0 || config.jerk > 50000.0f) {
        result.AddError(FormatRangeError("加加速度", config.jerk, 0.0f, 50000.0f));
    }

    // 验证距离参数
    if (!ValidateDistance(config.offset, -100.0f, 100.0f)) {
        result.AddError(FormatRangeError("回零偏移", config.offset, -100.0f, 100.0f));
    }

    if (config.search_distance <= 0.0f) {
        result.AddError("搜索距离必须大于0");
    } else if (!ValidateDistance(config.search_distance, -1000.0f, 1000.0f)) {
        result.AddError(FormatRangeError("搜索距离", config.search_distance, -1000.0f, 1000.0f));
    }

    if (config.escape_distance < 0.0f) {
        result.AddError("逃离距离必须大于或等于0");
    } else if (config.enable_escape && config.escape_distance <= 0.0f) {
        result.AddError("启用退离时，逃离距离必须大于0");
    } else if (!ValidateDistance(config.escape_distance, -100.0f, 100.0f)) {
        result.AddError(FormatRangeError("逃离距离", config.escape_distance, -100.0f, 100.0f));
    }

    if (config.escape_velocity < 0.0f ||
        (config.escape_velocity > 0.0f && !ValidateVelocity(config.escape_velocity, 0.1f, kHomingMaxVelocity))) {
        result.AddError(FormatRangeError("逃离速度", config.escape_velocity, 0.1f, kHomingMaxVelocity));
    }

    if (!config.home_backoff_enabled && config.mode == 2) {
        result.AddWarning("HOME+INDEX模式下禁用板卡二次回退，可能影响最终定位精度");
    }

    // 验证其他参数
    if (!ValidateRange(config.retry_count, 0, 10)) {
        result.AddError(FormatRangeError("重试次数", config.retry_count, 0, 10));
    }

    if (!ValidateRange(config.settle_time_ms, 0, 10000)) {
        result.AddError(FormatRangeError("稳定时间", config.settle_time_ms, 0, 10000));
    }

    if (config.escape_timeout_ms < 0 ||
        (config.escape_timeout_ms > 0 && !ValidateTimeout(config.escape_timeout_ms))) {
        result.AddError(FormatRangeError("逃离超时", config.escape_timeout_ms, 100, 300000));
    }

    const float32 effective_ready_zero_speed = config.ready_zero_speed_mm_s;
    if (config.search_distance > 0.0f && effective_ready_zero_speed > 0.0f) {
        const auto travel_timeout_ms = static_cast<int32>(
            std::ceil(static_cast<double>(config.search_distance) / static_cast<double>(effective_ready_zero_speed) *
                      1000.0));
        const int32 minimum_timeout_ms = travel_timeout_ms + std::max<int32>(config.settle_time_ms, 0);
        if (config.timeout_ms < minimum_timeout_ms) {
            result.AddError("超时时间不能小于搜索距离/回零速度 + 稳定时间: timeout_ms=" +
                            std::to_string(config.timeout_ms) +
                            ", minimum_required_ms=" + std::to_string(minimum_timeout_ms));
        }
    }

    if (!ValidateRange(config.escape_max_attempts, 1, 3)) {
        result.AddError(FormatRangeError("逃离次数", config.escape_max_attempts, 1, 3));
    }

    // 检查模式兼容性
    if (!ValidateModeCompatibility(config.mode, config.enable_limit_switch, config.enable_index)) {
        result.AddError("回零模式与启用开关设置不兼容");
    }

    // 添加警告信息
    if (config.rapid_velocity > kHomingMaxVelocity) {
        result.AddWarning("快速回零速度较高，建议降低到10mm/s以下");
    }

    if (config.timeout_ms < 10000) {
        result.AddWarning("超时时间较短，可能导致回零过程中超时");
    }

    if (config.search_distance < 50.0f) {
        result.AddWarning("搜索距离较短，可能无法找到回零位置");
    }

    if (config.home_debounce_ms == 0) {
        result.AddWarning("HOME去抖为0，可能导致开关抖动影响回零判定");
    }

    if (!config.home_backoff_enabled) {
        result.AddWarning("已禁用板卡原生HOME二次回退，回零成功后可能保持HOME触发态");
    }

    if (config.home_input_bit < 0) {
        result.AddWarning("HOME输入位未显式配置，默认使用轴号");
    }

    if (config.home_input_source != 3) {
        result.AddWarning("HOME输入源非MC_HOME(3)，仅影响诊断/退离判断，硬件回零仍使用HOME输入");
    }

    return result;
}

ValidationResult ConfigValidator::ValidateHomingConfigDetailed(const HomingConfig& config,
                                                               const MachineConfig& machine,
                                                               int axis_count) {
    ValidationResult result = ValidateHomingConfigDetailed(config);

    if (axis_count < 1) {
        axis_count = 1;
    }
    if (config.axis < 0 || config.axis >= axis_count) {
        result.AddError("轴编号超出有效范围 (0-" + std::to_string(axis_count - 1) + ")");
        return result;
    }

    constexpr float32 kSearchDistanceMarginMm = 5.0f;
    float32 axis_min = 0.0f;
    float32 axis_max = 0.0f;
    const char* axis_name = "未知";
    switch (config.axis) {
        case 0:
            axis_min = machine.soft_limits.x_min;
            axis_max = machine.soft_limits.x_max;
            axis_name = "X";
            break;
        case 1:
            axis_min = machine.soft_limits.y_min;
            axis_max = machine.soft_limits.y_max;
            axis_name = "Y";
            break;
        case 2:
            axis_min = machine.soft_limits.z_min;
            axis_max = machine.soft_limits.z_max;
            axis_name = "Z";
            break;
        default:
            break;
    }

    const float32 axis_range = axis_max - axis_min;
    if (axis_range <= 0.0f) {
        result.AddError(std::string("轴") + axis_name +
                        "软限位范围无效 (min=" + std::to_string(axis_min) +
                        ", max=" + std::to_string(axis_max) + ")");
    }

    if (axis_range > 0.0f) {
        const float32 max_search_distance = axis_range + kSearchDistanceMarginMm;
        if (config.search_distance > max_search_distance) {
            result.AddError(std::string("搜索距离超出行程上限 (轴") + axis_name +
                            " 搜索=" + std::to_string(config.search_distance) +
                            "mm, 上限=" + std::to_string(max_search_distance) + "mm)");
        }
    }

    const float32 effective_ready_zero_speed = config.ready_zero_speed_mm_s;
    if (machine.max_speed > 0.0f &&
        effective_ready_zero_speed > 0.0f &&
        effective_ready_zero_speed > machine.max_speed) {
        result.AddError("统一回零速度超出 machine.max_speed: speed=" +
                        std::to_string(effective_ready_zero_speed) +
                        "mm/s, max_speed=" + std::to_string(machine.max_speed) + "mm/s");
    }

    return result;
}

bool ConfigValidator::ValidateVelocity(float32 velocity, float32 min_val, float32 max_val) {
    return velocity > min_val && velocity <= max_val;
}

bool ConfigValidator::ValidateVelocityRelationship(float32 rapid, float32 locate, float32 index) {
    return (rapid > locate) && (locate > index) && (rapid > index);
}

bool ConfigValidator::ValidateAcceleration(float32 acceleration, float32 min_val, float32 max_val) {
    return acceleration >= min_val && acceleration <= max_val;
}

bool ConfigValidator::ValidateDistance(float32 distance, float32 min_val, float32 max_val) {
    return distance >= min_val && distance <= max_val;
}

bool ConfigValidator::ValidateTimeout(int32 timeout_ms, int32 min_val, int32 max_val) {
    return timeout_ms >= min_val && timeout_ms <= max_val;
}

bool ConfigValidator::ValidateRange(int32 value, int32 min_val, int32 max_val) {
    return value >= min_val && value <= max_val;
}

bool ConfigValidator::ValidateRange(float32 value, float32 min_val, float32 max_val) {
    return value >= min_val && value <= max_val;
}

std::string ConfigValidator::GetVelocitySuggestion(float32 current, float32 recommended) {
    std::ostringstream oss;
    oss << "当前速度 " << std::fixed << std::setprecision(1) << current << " mm/s，建议调整为 " << recommended
        << " mm/s";
    return oss.str();
}

std::string ConfigValidator::GetAccelerationSuggestion(float32 current, float32 recommended) {
    std::ostringstream oss;
    oss << "当前加速度 " << std::fixed << std::setprecision(1) << current << " mm/s²，建议调整为 " << recommended
        << " mm/s²";
    return oss.str();
}

std::string ConfigValidator::GetTimeoutSuggestion(int32 current, int32 recommended) {
    std::ostringstream oss;
    oss << "当前超时时间 " << current << " ms，建议调整为 " << recommended << " ms";
    return oss.str();
}

bool ConfigValidator::ValidateModeCompatibility(int32 mode, bool enable_limit_switch, bool enable_index) {
    switch (mode) {
        case 0:  // 限位回零
            return enable_limit_switch;
        case 1:  // HOME信号回零
        case 2:  // HOME+Z相信号回零
            return true;
        case 3:  // 索引回零
        case 6:  // 编码器Z相回零
            return enable_index;
        case 4:  // 限位+HOME回零
            return enable_limit_switch;
        case 5:  // 限位+HOME+Z相回零
            return enable_limit_switch;
        case 7:  // 绝对位置回零
        case 8:  // 软限位回零
        case 9:  // 自定义回零
            return true;
        default:
            return false;
    }
}

std::string ConfigValidator::FormatRangeError(const std::string& param_name,
                                              float32 value,
                                              float32 min_val,
                                              float32 max_val) {
    std::ostringstream oss;
    oss << param_name << " 值 " << std::fixed << std::setprecision(2) << value << " 超出有效范围 [" << min_val << ", "
        << max_val << "]";
    return oss.str();
}

std::string ConfigValidator::FormatRangeError(const std::string& param_name,
                                              int32 value,
                                              int32 min_val,
                                              int32 max_val) {
    std::ostringstream oss;
    oss << param_name << " 值 " << value << " 超出有效范围 [" << min_val << ", " << max_val << "]";
    return oss.str();
}

// === 最佳实践验证实现 ===

ValidationResult ConfigValidator::ValidateSeamlessUnderwearBestPractices(const DispensingConfig& config) {
    ValidationResult result;
    using namespace Domain::Configuration::Ports::SeamlessUnderwearDefaults;

    // 温度验证（PUR胶工艺）
    if (config.temperature_target_c > 0.0f) {
        if (config.temperature_target_c < 130.0f) {
            result.AddWarning("温度偏低: " + std::to_string(static_cast<int>(config.temperature_target_c))
                + "C, PUR胶推荐130-150C");
        }
        if (config.temperature_tolerance_c > 10.0f) {
            result.AddWarning("温度容差过宽: " + std::to_string(static_cast<int>(config.temperature_tolerance_c))
                + "C, 推荐5C以保证黏度稳定");
        }
    }

    // 黏度验证
    if (config.viscosity_target_cps > 0.0f) {
        if (config.viscosity_target_cps > 10000.0f) {
            result.AddWarning("黏度偏高: " + std::to_string(static_cast<int>(config.viscosity_target_cps))
                + " cP, 可能导致拉丝, 推荐3000-8000 cP");
        }
        if (config.viscosity_tolerance_pct > 20.0f) {
            result.AddWarning("黏度容差过宽: " + std::to_string(static_cast<int>(config.viscosity_tolerance_pct))
                + "%, 推荐15%以内");
        }
    }

    if (config.corner_pulse_scale < 0.5f || config.corner_pulse_scale > 1.5f) {
        result.AddWarning("拐角胶量缩放系数偏离推荐范围(0.5-1.5)，可能影响拐角线宽一致性");
    }

    if (config.curvature_speed_factor < 0.5f || config.curvature_speed_factor > 1.0f) {
        result.AddWarning("曲率限速系数偏离推荐范围(0.5-1.0)，可能影响拐角平滑度");
    }
    if (config.trajectory_sample_dt > 0.02f) {
        result.AddWarning("轨迹采样时间步长偏大(>0.02s)，可能影响轨迹平滑度");
    }
    if (config.trajectory_sample_ds > 1.0f) {
        result.AddWarning("轨迹采样弧长步长偏大(>1.0mm)，可能影响轨迹精度");
    }

    return result;
}

ValidationResult ConfigValidator::ValidateMotionBestPractices(const MachineConfig& config) {
    ValidationResult result;
    using namespace Domain::Configuration::Ports::SeamlessUnderwearDefaults;

    if (config.max_speed < 50.0f) {
        result.AddWarning("最大速度偏低: " + std::to_string(static_cast<int>(config.max_speed))
            + " mm/s, 生产效率受限, 行业标杆100-800 mm/s");
    }

    if (config.max_acceleration < 100.0f) {
        result.AddWarning("最大加速度偏低: " + std::to_string(static_cast<int>(config.max_acceleration))
            + " mm/s2, 响应速度受限, 推荐500-2000 mm/s2");
    }

    if (config.positioning_tolerance > kPositioningTolerance) {
        result.AddWarning("定位精度偏低: " + std::to_string(config.positioning_tolerance)
            + " mm, 无痕内衣推荐0.05mm以内");
    }

    return result;
}

}  // namespace Siligen
