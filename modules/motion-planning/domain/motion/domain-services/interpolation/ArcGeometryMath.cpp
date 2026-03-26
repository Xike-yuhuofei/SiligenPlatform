/**
 * @file ArcGeometryMath.cpp
 * @brief 圆弧数学工具实现
 */

#include "ArcGeometryMath.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion {

/**
 * @brief 计算圆弧上的点
 */
Point2D ArcGeometryMath::CalculatePointOnArc(const Point2D& center, float32 radius, float32 angle) const {
    Point2D result;
    result.x = center.x + radius * std::cos(angle);
    result.y = center.y + radius * std::sin(angle);
    return result;
}

/**
 * @brief 计算圆弧长度
 */
float32 ArcGeometryMath::CalculateArcLength(float32 radius, float32 angle_span) const {
    return radius * std::abs(angle_span);
}

/**
 * @brief 角度归一化到[-π, π]
 */
float32 ArcGeometryMath::NormalizeAngle(float32 angle) const {
    constexpr float32 PI = 3.14159265358979323846f;
    constexpr float32 TWO_PI = 2.0f * PI;

    // 归一化到[-π, π]
    while (angle > PI) {
        angle -= TWO_PI;
    }
    while (angle < -PI) {
        angle += TWO_PI;
    }

    return angle;
}

/**
 * @brief 计算两点间角度差
 */
float32 ArcGeometryMath::CalculateAngleDifference(float32 start_angle,
                                               float32 end_angle,
                                               ArcInterpolator::ArcDirection direction) const {
    constexpr float32 PI = 3.14159265358979323846f;
    constexpr float32 TWO_PI = 2.0f * PI;

    // 归一化角度
    start_angle = NormalizeAngle(start_angle);
    end_angle = NormalizeAngle(end_angle);

    float32 angle_diff = end_angle - start_angle;

    if (direction == ArcInterpolator::ArcDirection::COUNTER_CLOCKWISE) {
        // 逆时针方向
        if (angle_diff < 0) {
            angle_diff += TWO_PI;
        }
    } else {
        // 顺时针方向
        if (angle_diff > 0) {
            angle_diff -= TWO_PI;
        }
        angle_diff = std::abs(angle_diff);
    }

    return angle_diff;
}

/**
 * @brief 优化圆弧速度分布
 */
std::vector<float32> ArcGeometryMath::OptimizeArcVelocity(const ArcInterpolator::ArcParameters& params,
                                                       const InterpolationConfig& config) const {
    std::vector<float32> velocity_profile;

    // 计算圆弧长度
    float32 arc_length = CalculateArcLength(params.radius, params.angle_span);

    // 计算向心加速度限制下的最大速度
    // a_centripetal = v^2 / r
    // v_max = sqrt(a_max * r)
    float32 centripetal_velocity_limit = std::sqrt(config.max_acceleration * params.radius);

    // 取配置速度和向心加速度限制中的较小值
    float32 safe_velocity = std::min(config.max_velocity, centripetal_velocity_limit);

    // 计算采样点数量
    int32 num_samples = static_cast<int32>(arc_length / (safe_velocity * config.time_step)) + 1;
    if (num_samples < 2) {
        num_samples = 2;
    }

    velocity_profile.reserve(num_samples);

    // 简单的梯形速度规划
    // 加速段、匀速段、减速段
    float32 accel_distance = (safe_velocity * safe_velocity) / (2.0f * config.max_acceleration);
    float32 decel_distance = accel_distance;
    float32 const_velocity_distance = arc_length - accel_distance - decel_distance;

    if (const_velocity_distance < 0) {
        // 路径太短，无法达到最大速度
        // 使用三角形速度规划
        float32 peak_velocity = std::sqrt(config.max_acceleration * arc_length);
        peak_velocity = std::min(peak_velocity, safe_velocity);
        accel_distance = arc_length / 2.0f;
        decel_distance = arc_length / 2.0f;
        const_velocity_distance = 0;

        for (int32 i = 0; i < num_samples; ++i) {
            float32 position = (arc_length * i) / (num_samples - 1);
            float32 velocity;

            if (position <= accel_distance) {
                // 加速段
                float32 ratio = position / accel_distance;
                velocity = peak_velocity * ratio;
            } else {
                // 减速段
                float32 ratio = (arc_length - position) / decel_distance;
                velocity = peak_velocity * ratio;
            }

            velocity_profile.push_back(velocity);
        }
    } else {
        // 标准梯形速度规划
        for (int32 i = 0; i < num_samples; ++i) {
            float32 position = (arc_length * i) / (num_samples - 1);
            float32 velocity;

            if (position <= accel_distance) {
                // 加速段
                float32 ratio = position / accel_distance;
                velocity = safe_velocity * ratio;
            } else if (position <= accel_distance + const_velocity_distance) {
                // 匀速段
                velocity = safe_velocity;
            } else {
                // 减速段
                float32 decel_position = position - (accel_distance + const_velocity_distance);
                float32 ratio = 1.0f - (decel_position / decel_distance);
                velocity = safe_velocity * ratio;
            }

            velocity_profile.push_back(velocity);
        }
    }

    return velocity_profile;
}

/**
 * @brief 计算圆弧点胶触发位置
 */
std::vector<float32> ArcGeometryMath::CalculateArcTriggerPositions(const ArcInterpolator::ArcParameters& params,
                                                                const InterpolationConfig& config) const {
    std::vector<float32> trigger_positions;

    // 计算圆弧长度
    float32 arc_length = CalculateArcLength(params.radius, params.angle_span);

    // 默认触发间隔 (可以从config中获取，这里使用合理的默认值)
    // 假设每隔1mm触发一次
    constexpr float32 DEFAULT_TRIGGER_INTERVAL_MM = 1.0f;

    // 计算触发点数量
    int32 num_triggers = static_cast<int32>(arc_length / DEFAULT_TRIGGER_INTERVAL_MM);

    if (num_triggers <= 0) {
        // 如果圆弧太短，至少在起点和终点触发
        trigger_positions.push_back(0.0f);
        trigger_positions.push_back(arc_length);
        return trigger_positions;
    }

    // 生成均匀分布的触发位置
    trigger_positions.reserve(num_triggers + 2);

    // 起点触发
    trigger_positions.push_back(0.0f);

    // 中间触发点
    for (int32 i = 1; i <= num_triggers; ++i) {
        float32 trigger_position = i * DEFAULT_TRIGGER_INTERVAL_MM;
        if (trigger_position < arc_length) {
            trigger_positions.push_back(trigger_position);
        }
    }

    // 终点触发
    if (trigger_positions.back() < arc_length) {
        trigger_positions.push_back(arc_length);
    }

    return trigger_positions;
}

}  // namespace Siligen::Domain::Motion
