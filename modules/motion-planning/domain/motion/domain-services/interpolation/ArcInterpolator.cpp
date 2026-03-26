/**
 * @file ArcInterpolator.cpp
 * @brief 圆弧插补优化算法实现
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#define MODULE_NAME "ArcInterpolator"
#include "ArcInterpolator.h"

#include "ArcGeometryMath.h"
#include "CircleCalculator.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Siligen::Domain::Motion {

using Siligen::Shared::Types::kPi;

ArcInterpolator::ArcInterpolator()
    : TrajectoryInterpolatorBase(),
      m_math_utils(std::make_unique<ArcGeometryMath>()),
      m_circle_calculator(std::make_unique<CircleCalculator>()) {
    m_arc_cache.reserve(100);
    m_angle_cache.reserve(1000);
}

ArcInterpolator::~ArcInterpolator() = default;

std::vector<TrajectoryPoint> ArcInterpolator::CalculateInterpolation(const std::vector<Point2D>& points,
                                                                     const InterpolationConfig& config) {
    if (!ValidateParameters(points, config)) {
        SILIGEN_LOG_ERROR("圆弧插补参数验证失败");
        return {};
    }

    if (points.size() == 3) {
        // 三点圆弧插补
        return ThreePointArcInterpolation(points[0], points[1], points[2], config);
    } else if (points.size() == 2) {
        // 默认创建半圆弧
        Point2D midpoint = (points[0] + points[1]) * 0.5f;
        float32 distance = points[0].DistanceTo(points[1]);
        Point2D perpendicular = Point2D(-(points[1].y - points[0].y), points[1].x - points[0].x).Normalized();
        Point2D arc_midpoint = midpoint + perpendicular * distance * 0.5f;

        return ThreePointArcInterpolation(points[0], arc_midpoint, points[1], config);
    } else {
        SILIGEN_LOG_ERROR("圆弧插补需要2个或3个点");
        return {};
    }
}

std::vector<TrajectoryPoint> ArcInterpolator::ThreePointArcInterpolation(const Point2D& p1,
                                                                         const Point2D& p2,
                                                                         const Point2D& p3,
                                                                         const InterpolationConfig& config) {
    // 计算圆弧参数
    ArcParameters params = CalculateArcParameters(p1, p2, p3);

    if (!ValidateArcParameters(params, config)) {
        SILIGEN_LOG_ERROR("三点圆弧参数计算失败");
        return {};
    }

    return GenerateArcTrajectory(params, config);
}

std::vector<TrajectoryPoint> ArcInterpolator::CenterArcInterpolation(const Point2D& center,
                                                                     const Point2D& start_point,
                                                                     const Point2D& end_point,
                                                                     ArcDirection direction,
                                                                     const InterpolationConfig& config) {
    // 计算圆弧参数
    ArcParameters params = CalculateArcParameters(center, start_point, end_point, direction);

    if (!ValidateArcParameters(params, config)) {
        SILIGEN_LOG_ERROR("圆心圆弧参数计算失败");
        return {};
    }

    return GenerateArcTrajectory(params, config);
}

std::vector<TrajectoryPoint> ArcInterpolator::HelicalInterpolation(const HelixParameters& helix_params,
                                                                   const InterpolationConfig& config) {
    if (helix_params.radius <= 0.0f || helix_params.revolutions <= 0.0f) {
        SILIGEN_LOG_ERROR("螺旋线参数无效");
        return {};
    }

    return GenerateHelixTrajectory(helix_params, config);
}

std::vector<TrajectoryPoint> ArcInterpolator::VariableRadiusArcInterpolation(const Point2D& start_point,
                                                                             const Point2D& end_point,
                                                                             float32 start_radius,
                                                                             float32 end_radius,
                                                                             const Point2D& center,
                                                                             const InterpolationConfig& config) {
    // 计算起始和结束角度
    float32 start_angle = std::atan2(start_point.y - center.y, start_point.x - center.x);
    float32 end_angle = std::atan2(end_point.y - center.y, end_point.x - center.x);

    float32 angle_span = m_math_utils->NormalizeAngle(end_angle - start_angle);
    if (angle_span < 0.0f) {
        angle_span += 2.0f * kPi;
    }

    std::vector<TrajectoryPoint> trajectory;
    int32 num_points = static_cast<int32>(std::ceil(angle_span / (config.time_step * 10.0f)));

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(num_points);
        float32 current_angle = start_angle + t * angle_span;
        float32 current_radius = start_radius + t * (end_radius - start_radius);

        Point2D current_pos = m_math_utils->CalculatePointOnArc(center, current_radius, current_angle);

        TrajectoryPoint point;
        point.position = current_pos;
        point.timestamp = t * (angle_span / config.max_velocity);
        point.sequence_id = static_cast<uint32>(i);

        // 变半径圆弧需要调整速度
        float32 circumference = 2.0f * kPi * current_radius;
        point.velocity = config.max_velocity * (current_radius / std::max(start_radius, end_radius));

        trajectory.push_back(point);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> ArcInterpolator::LineArcBlend(const Point2D& line_end,
                                                           const Point2D& arc_start,
                                                           const Point2D& arc_center,
                                                           float32 blend_radius,
                                                           const InterpolationConfig& config) {
    // 计算过渡圆弧的参数
    Point2D to_arc = (arc_start - arc_center).Normalized();
    Point2D to_line = (line_end - arc_start).Normalized();

    // 计算过渡圆弧的起点和终点
    Point2D blend_start = arc_start - to_arc * blend_radius;
    Point2D blend_end = arc_start + to_line * blend_radius;

    // 计算过渡圆弧的圆心
    float32 angle = std::acos(to_arc.Dot(to_line));
    Point2D bisector = (to_arc + to_line).Normalized();
    Point2D blend_center = arc_start + bisector * (blend_radius / std::sin(angle / 2.0f));

    // 生成过渡圆弧
    return CenterArcInterpolation(blend_center, blend_start, blend_end, ArcDirection::COUNTER_CLOCKWISE, config);
}

ArcInterpolator::ArcParameters ArcInterpolator::CalculateArcParameters(const Point2D& p1,
                                                                       const Point2D& p2,
                                                                       const Point2D& p3) const {
    return m_circle_calculator->CalculateArcParameters(p1, p2, p3);
}

ArcInterpolator::ArcParameters ArcInterpolator::CalculateArcParameters(const Point2D& center,
                                                                       const Point2D& start_point,
                                                                       const Point2D& end_point,
                                                                       ArcDirection direction) const {
    return m_circle_calculator->CalculateArcParameters(center, start_point, end_point, direction);
}

bool ArcInterpolator::ValidateArcParameters(const ArcParameters& params, const InterpolationConfig& config) const {
    return m_circle_calculator->ValidateArcParameters(params, config);
}

std::vector<TrajectoryPoint> ArcInterpolator::GenerateArcTrajectory(const ArcParameters& params,
                                                                    const InterpolationConfig& config) const {
    std::vector<TrajectoryPoint> trajectory;

    float32 arc_length = m_math_utils->CalculateArcLength(params.radius, params.angle_span);
    float32 total_time = arc_length / config.max_velocity;

    // 优化速度分布
    std::vector<float32> velocity_profile = m_math_utils->OptimizeArcVelocity(params, config);

    // 计算插补点数
    int32 num_points = static_cast<int32>(std::ceil(total_time / config.time_step));

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(num_points);
        float32 current_angle = params.start_angle + t * params.angle_span;

        Point2D current_pos = m_math_utils->CalculatePointOnArc(params.center, params.radius, current_angle);

        TrajectoryPoint point;
        point.position = current_pos;
        point.timestamp = t * total_time;
        point.sequence_id = static_cast<uint32>(i);

        // 设置速度
        if (i < static_cast<int32>(velocity_profile.size())) {
            point.velocity = velocity_profile[i];
        } else {
            point.velocity = config.max_velocity;
        }

        // 计算向心加速度
        point.acceleration = point.velocity * point.velocity / params.radius;

        trajectory.push_back(point);
    }

    // 设置点胶触发位置
    if (config.enable_look_ahead) {
        std::vector<float32> trigger_positions = m_math_utils->CalculateArcTriggerPositions(params, config);
        for (float32 trigger_pos : trigger_positions) {
            int32 trigger_index = static_cast<int32>(trigger_pos * num_points);
            if (trigger_index < static_cast<int32>(trajectory.size())) {
                trajectory[trigger_index].enable_position_trigger = true;
                trajectory[trigger_index].trigger_position_mm = trigger_pos * arc_length;
                trajectory[trigger_index].trigger_pulse_width_us = 2000;  // 默认2ms
            }
        }
    }

    return trajectory;
}

std::vector<TrajectoryPoint> ArcInterpolator::GenerateHelixTrajectory(const HelixParameters& params,
                                                                      const InterpolationConfig& config) const {
    std::vector<TrajectoryPoint> trajectory;

    float32 total_angle = params.revolutions * 2.0f * kPi;
    float32 helix_length = std::sqrt(std::pow(2.0f * kPi * params.radius * params.revolutions, 2) +
                                     std::pow(params.end_z - params.start_z, 2));
    float32 total_time = helix_length / config.max_velocity;

    int32 num_points = static_cast<int32>(std::ceil(total_time / config.time_step));

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(num_points);
        float32 current_angle = params.start_angle + t * total_angle;
        float32 current_z = params.start_z + t * (params.end_z - params.start_z);

        Point2D current_pos = m_math_utils->CalculatePointOnArc(params.center, params.radius, current_angle);

        TrajectoryPoint point;
        point.position = current_pos;
        point.timestamp = t * total_time;
        point.sequence_id = static_cast<uint32>(i);
        point.velocity = config.max_velocity;

        // 螺旋线需要考虑Z轴运动
        float32 z_velocity = (params.end_z - params.start_z) / total_time;
        float32 tangential_velocity = 2.0f * kPi * params.radius * params.revolutions / total_time;
        point.velocity = std::sqrt(tangential_velocity * tangential_velocity + z_velocity * z_velocity);

        trajectory.push_back(point);
    }

    return trajectory;
}

Point2D ArcInterpolator::CalculatePointOnArc(const Point2D& center, float32 radius, float32 angle) const {
    return m_math_utils->CalculatePointOnArc(center, radius, angle);
}

float32 ArcInterpolator::CalculateArcLength(float32 radius, float32 angle_span) const {
    return m_math_utils->CalculateArcLength(radius, angle_span);
}

float32 ArcInterpolator::NormalizeAngle(float32 angle) const {
    return m_math_utils->NormalizeAngle(angle);
}

float32 ArcInterpolator::CalculateAngleDifference(float32 start_angle,
                                                  float32 end_angle,
                                                  ArcDirection direction) const {
    return m_math_utils->CalculateAngleDifference(start_angle, end_angle, direction);
}

std::vector<float32> ArcInterpolator::OptimizeArcVelocity(const ArcParameters& params,
                                                          const InterpolationConfig& config) const {
    return m_math_utils->OptimizeArcVelocity(params, config);
}

std::vector<float32> ArcInterpolator::CalculateArcTriggerPositions(const ArcParameters& params,
                                                                   const InterpolationConfig& config) const {
    return m_math_utils->CalculateArcTriggerPositions(params, config);
}

}  // namespace Siligen::Domain::Motion

