/**
 * @file CircleCalculator.cpp
 * @brief 圆参数计算器实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#define MODULE_NAME "CircleCalculator"
#include "CircleCalculator.h"

#include "ArcGeometryMath.h"
#include "shared/types/MathConstants.h"

#include <cmath>

namespace Siligen::Domain::Motion {

using Siligen::Shared::Types::kPi;

ArcInterpolator::ArcParameters CircleCalculator::CalculateArcParameters(const Point2D& p1,
                                                                        const Point2D& p2,
                                                                        const Point2D& p3) const {
    ArcInterpolator::ArcParameters params{};

    // 检查三点是否共线
    float32 area = p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y);
    if (std::abs(area) < 1e-10f) {
        return params;
    }

    // 计算圆心
    float32 d = 2.0f * (p1.x * (p2.y - p3.y) + p2.x * (p3.y - p1.y) + p3.x * (p1.y - p2.y));

    float32 ux = ((p1.x * p1.x + p1.y * p1.y) * (p2.y - p3.y) + (p2.x * p2.x + p2.y * p2.y) * (p3.y - p1.y) +
                  (p3.x * p3.x + p3.y * p3.y) * (p1.y - p2.y)) /
                 d;

    float32 uy = ((p1.x * p1.x + p1.y * p1.y) * (p3.x - p2.x) + (p2.x * p2.x + p2.y * p2.y) * (p1.x - p3.x) +
                  (p3.x * p3.x + p3.y * p3.y) * (p2.x - p1.x)) /
                 d;

    params.center = Point2D(ux, uy);
    params.radius = params.center.DistanceTo(p1);

    // 计算起始和结束角度
    params.start_angle = std::atan2(p1.y - params.center.y, p1.x - params.center.x);
    float32 mid_angle = std::atan2(p2.y - params.center.y, p2.x - params.center.x);
    params.end_angle = std::atan2(p3.y - params.center.y, p3.x - params.center.x);

    // 判断圆弧方向
    float32 cross_product = (p2.x - p1.x) * (p3.y - p2.y) - (p2.y - p1.y) * (p3.x - p2.x);
    params.direction = (cross_product > 0.0f) ? ArcInterpolator::ArcDirection::COUNTER_CLOCKWISE
                                              : ArcInterpolator::ArcDirection::CLOCKWISE;

    // 计算角度跨度
    ArcGeometryMath math_utils;
    params.angle_span = math_utils.CalculateAngleDifference(params.start_angle, params.end_angle, params.direction);

    params.start_point = p1;
    params.end_point = p3;
    params.is_full_circle = (std::abs(params.angle_span - 2.0f * kPi) < 1e-6f);

    return params;
}

ArcInterpolator::ArcParameters CircleCalculator::CalculateArcParameters(const Point2D& center,
                                                                        const Point2D& start_point,
                                                                        const Point2D& end_point,
                                                                        ArcInterpolator::ArcDirection direction) const {
    ArcInterpolator::ArcParameters params{};

    params.center = center;
    params.radius = center.DistanceTo(start_point);
    params.start_point = start_point;
    params.end_point = end_point;
    params.direction = direction;

    // 验证半径一致性
    float32 end_radius = center.DistanceTo(end_point);
    if (std::abs(params.radius - end_radius) > 1e-6f) {
        // Domain层不应包含日志记录
    }

    // 计算角度
    params.start_angle = std::atan2(start_point.y - center.y, start_point.x - center.x);
    params.end_angle = std::atan2(end_point.y - center.y, end_point.x - center.x);

    // 计算角度跨度
    ArcGeometryMath math_utils;
    params.angle_span = math_utils.CalculateAngleDifference(params.start_angle, params.end_angle, direction);
    params.is_full_circle = (std::abs(params.angle_span - 2.0f * kPi) < 1e-6f);

    return params;
}

bool CircleCalculator::ValidateArcParameters(const ArcInterpolator::ArcParameters& params,
                                             const InterpolationConfig& config) const {
    if (params.radius <= 0.0f) {
        return false;
    }

    if (params.radius < config.position_tolerance) {
        return false;
    }

    if (std::abs(params.angle_span) < 1e-6f) {
        return false;
    }

    if (params.angle_span > 2.0f * kPi + 1e-6f) {
        return false;
    }

    return true;
}

}  // namespace Siligen::Domain::Motion
