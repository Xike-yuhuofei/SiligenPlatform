#include "domain/safety/domain-services/SoftLimitValidator.h"

#include <cmath>
#include <cstring>

namespace Siligen::Domain::Safety::DomainServices {

using namespace Shared::Types;

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
    return std::isfinite(value);
}

Result<void> SoftLimitValidator::ValidateAxisLimit(LogicalAxisId axis_id,
                                                   float32 min_val,
                                                   float32 max_val,
                                                   bool enabled) noexcept {
    if (!enabled) {
        return Result<void>::Success();
    }

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

    if (FloatGreaterOrEqual(min_val, max_val)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_CONFIG_VALUE,
            "Axis " + axis_label + " soft limit min (" +
                std::to_string(min_val) + ") >= max (" + std::to_string(max_val) + ")"));
    }

    return Result<void>::Success();
}

Result<void> SoftLimitValidator::ValidateConfig(const MotionConfiguration& config) noexcept {
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

Result<void> SoftLimitValidator::ValidatePoint(const Point2D& point,
                                               const MotionConfiguration& config) noexcept {
    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }

    if (!config.axes.empty()) {
        const auto& x_axis = config.axes[0];
        if (x_axis.softLimits.enabled) {
            const float32 x_min = static_cast<float32>(x_axis.softLimits.min);
            const float32 x_max = static_cast<float32>(x_axis.softLimits.max);
            if (!FloatInRange(point.x, x_min, x_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "X axis position " + std::to_string(point.x) +
                        " exceeds soft limit range [" + std::to_string(x_min) +
                        ", " + std::to_string(x_max) + "]"));
            }
        }
    }

    if (config.axes.size() > 1) {
        const auto& y_axis = config.axes[1];
        if (y_axis.softLimits.enabled) {
            const float32 y_min = static_cast<float32>(y_axis.softLimits.min);
            const float32 y_max = static_cast<float32>(y_axis.softLimits.max);
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

Result<void> SoftLimitValidator::ValidateTrajectory(const TrajectoryPoint* trajectory,
                                                    size_t count,
                                                    const MotionConfiguration& config) noexcept {
    if (trajectory == nullptr || count == 0) {
        return Result<void>::Success();
    }

    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }

    for (size_t i = 0; i < count; ++i) {
        const auto& point = trajectory[i];
        auto result = ValidatePoint(point.position, config);
        if (!result.IsSuccess()) {
            return Result<void>::Failure(Error(
                result.GetError().GetCode(),
                "Trajectory point " + std::to_string(i) + ": " + result.GetError().GetMessage()));
        }
    }

    return Result<void>::Success();
}

Result<void> SoftLimitValidator::ValidateTrajectory(const TrajectoryPoint* trajectory,
                                                    size_t count,
                                                    const MotionConfiguration& config,
                                                    const AxisConfiguration* axis_configs,
                                                    size_t axis_count) noexcept {
    if (trajectory == nullptr || count == 0) {
        return Result<void>::Success();
    }

    if (axis_configs == nullptr || axis_count == 0) {
        return ValidateTrajectory(trajectory, count, config);
    }

    auto config_result = ValidateConfig(config);
    if (!config_result.IsSuccess()) {
        return config_result;
    }

    for (size_t i = 0; i < count; ++i) {
        const auto& point = trajectory[i].position;

        if (axis_count > 0 && axis_configs[0].softLimits.enabled) {
            const float32 x_min = static_cast<float32>(axis_configs[0].softLimits.min);
            const float32 x_max = static_cast<float32>(axis_configs[0].softLimits.max);
            if (!FloatInRange(point.x, x_min, x_max)) {
                return Result<void>::Failure(Error(
                    ErrorCode::POSITION_OUT_OF_RANGE,
                    "Trajectory point " + std::to_string(i) +
                        ": X axis position " + std::to_string(point.x) +
                        " exceeds soft limit range [" + std::to_string(x_min) +
                        ", " + std::to_string(x_max) + "]"));
            }
        }

        if (axis_count > 1 && axis_configs[1].softLimits.enabled) {
            const float32 y_min = static_cast<float32>(axis_configs[1].softLimits.min);
            const float32 y_max = static_cast<float32>(axis_configs[1].softLimits.max);
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

}  // namespace Siligen::Domain::Safety::DomainServices
