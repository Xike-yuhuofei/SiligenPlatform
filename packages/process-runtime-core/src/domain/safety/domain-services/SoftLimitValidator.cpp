#include "SoftLimitValidator.h"

#include <cmath>
#include <cstring>

namespace Siligen {
namespace Domain {
namespace Safety {
namespace DomainServices {

using namespace Shared::Types;

// ========== 浮点比较辅助函数 ==========

bool SoftLimitValidator::FloatLessOrEqual(float32 a, float32 b) noexcept {
    return a <= b + EPSILON;
}

bool SoftLimitValidator::FloatGreaterOrEqual(float32 a, float32 b) noexcept {
    return a >= b - EPSILON;
}

bool SoftLimitValidator::FloatInRange(float32 val, float32 min, float32 max) noexcept {
    return FloatGreaterOrEqual(val, min) && FloatLessOrEqual(val, max);
}

bool SoftLimitValidator::IsFinite(float32 value) noexcept {
    // 检查是否为有限值（非NaN非Inf）
    // IEEE 754: NaN != NaN, Inf == Inf
    return std::isfinite(value);
}

// ========== 配置验证 ==========

Result<void> SoftLimitValidator::ValidateAxisLimit(
    LogicalAxisId axis_id,
    float32 min_val,
    float32 max_val,
    bool enabled) noexcept {
    
    // 如果未启用，跳过验证
    if (!enabled) {
        return Result<void>::Success();
    }
    
    // 检查NaN和Inf
    const std::string axis_label =
        std::string(AxisName(axis_id)) + "(" + std::to_string(ToIndex(axis_id)) + ")";

    if (!IsFinite(min_val)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_CONFIG_VALUE,
            "Axis " + axis_label + " soft limit min is not finite (NaN or Inf)"));
    }

    if (!IsFinite(max_val)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_CONFIG_VALUE,
            "Axis " + axis_label + " soft limit max is not finite (NaN or Inf)"));
    }

    // 检查min < max
    if (FloatGreaterOrEqual(min_val, max_val)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_CONFIG_VALUE,
            "Axis " + axis_label + " soft limit min (" +
            std::to_string(min_val) + ") >= max (" + std::to_string(max_val) + ")"));
    }
    
    return Result<void>::Success();
}

Result<void> SoftLimitValidator::ValidateConfig(
    const MotionConfiguration& config) noexcept {
    
    // 验证每个轴的软限位配置
    for (const auto& axis : config.axes) {
        auto result = ValidateAxisLimit(
            axis.axisId,
            static_cast<float32>(axis.softLimits.min),
            static_cast<float32>(axis.softLimits.max),
            axis.softLimits.enabled);
        
        if (!result.IsSuccess()) {
            return result;
        }
    }
    
    return Result<void>::Success();
}

// ========== 点验证 ==========

Result<void> SoftLimitValidator::ValidatePoint(
    const Point2D& point,
    const MotionConfiguration& config) noexcept {
    
    // 首先验证配置
    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }
    
    // 验证X坐标（假设轴0是X轴）
    if (!config.axes.empty()) {
        const auto& x_axis = config.axes[0];
        if (x_axis.softLimits.enabled) {
            float32 x_min = static_cast<float32>(x_axis.softLimits.min);
            float32 x_max = static_cast<float32>(x_axis.softLimits.max);
            
            if (!FloatInRange(point.x, x_min, x_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "X axis position " + std::to_string(point.x) +
                    " exceeds soft limit range [" + std::to_string(x_min) +
                    ", " + std::to_string(x_max) + "]"));
            }
        }
    }
    
    // 验证Y坐标（假设轴1是Y轴）
    if (config.axes.size() > 1) {
        const auto& y_axis = config.axes[1];
        if (y_axis.softLimits.enabled) {
            float32 y_min = static_cast<float32>(y_axis.softLimits.min);
            float32 y_max = static_cast<float32>(y_axis.softLimits.max);
            
            if (!FloatInRange(point.y, y_min, y_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "Y axis position " + std::to_string(point.y) +
                    " exceeds soft limit range [" + std::to_string(y_min) +
                    ", " + std::to_string(y_max) + "]"));
            }
        }
    }
    
    return Result<void>::Success();
}

// ========== 轨迹验证（无轴配置） ==========

Result<void> SoftLimitValidator::ValidateTrajectory(
    const TrajectoryPoint* trajectory,
    size_t count,
    const MotionConfiguration& config) noexcept {
    
    // 空轨迹视为有效
    if (trajectory == nullptr || count == 0) {
        return Result<void>::Success();
    }
    
    // 首先验证配置
    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }
    
    // 验证每个轨迹点
    for (size_t i = 0; i < count; ++i) {
        const auto& point = trajectory[i];
        auto result = ValidatePoint(point.position, config);
        
        if (!result.IsSuccess()) {
            // 在错误消息中添加轨迹点索引
            return Result<void>::Failure(Error(
                result.GetError().GetCode(),
                "Trajectory point " + std::to_string(i) + ": " + result.GetError().GetMessage()));
        }
    }
    
    return Result<void>::Success();
}

// ========== 轨迹验证（带轴配置） ==========

Result<void> SoftLimitValidator::ValidateTrajectory(
    const TrajectoryPoint* trajectory,
    size_t count,
    const MotionConfiguration& config,
    const AxisConfiguration* axis_configs,
    size_t axis_count) noexcept {
    
    // 空轨迹视为有效
    if (trajectory == nullptr || count == 0) {
        return Result<void>::Success();
    }
    
    // 如果未提供轴配置，使用基础版本
    if (axis_configs == nullptr || axis_count == 0) {
        return ValidateTrajectory(trajectory, count, config);
    }
    
    // 首先验证配置
    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }
    
    // 验证每个轨迹点
    for (size_t i = 0; i < count; ++i) {
        const auto& traj_point = trajectory[i];
        const auto& point = traj_point.position;
        
        // 验证X坐标（轴0）
        if (axis_count > 0 && axis_configs[0].softLimits.enabled) {
            float32 x_min = static_cast<float32>(axis_configs[0].softLimits.min);
            float32 x_max = static_cast<float32>(axis_configs[0].softLimits.max);
            
            if (!FloatInRange(point.x, x_min, x_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "Trajectory point " + std::to_string(i) +
                    ": X axis position " + std::to_string(point.x) +
                    " exceeds soft limit range [" + std::to_string(x_min) +
                    ", " + std::to_string(x_max) + "]"));
            }
        }
        
        // 验证Y坐标（轴1）
        if (axis_count > 1 && axis_configs[1].softLimits.enabled) {
            float32 y_min = static_cast<float32>(axis_configs[1].softLimits.min);
            float32 y_max = static_cast<float32>(axis_configs[1].softLimits.max);
            
            if (!FloatInRange(point.y, y_min, y_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "Trajectory point " + std::to_string(i) +
                    ": Y axis position " + std::to_string(point.y) +
                    " exceeds soft limit range [" + std::to_string(y_min) +
                    ", " + std::to_string(y_max) + "]"));
            }
        }
    }
    
    return Result<void>::Success();
}

}  // namespace DomainServices
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
