/**
 * @file SplineInterpolator.cpp
 * @brief 三次样条插补算法实现
 *
 * @author Claude Code
 * @date 2025-10-30
 */

#include "SplineInterpolator.h"

#include "BSplineCalculator.h"
#include "BezierCalculator.h"
#include "SplineGeometryMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Siligen::Domain::Motion {

SplineInterpolator::SplineInterpolator()
    : TrajectoryInterpolatorBase(),
      m_math_utils(std::make_unique<SplineGeometryMath>()),
      m_bezier_calculator(std::make_unique<BezierCalculator>()),
      m_bspline_calculator(std::make_unique<BSplineCalculator>()) {
    m_spline_cache.reserve(100);
    m_bezier_cache.reserve(1000);
    m_basis_cache.reserve(1000);
}

SplineInterpolator::~SplineInterpolator() = default;

std::vector<TrajectoryPoint> SplineInterpolator::CalculateInterpolation(const std::vector<Point2D>& points,
                                                                        const InterpolationConfig& config) {
    if (!ValidateParameters(points, config)) {
        return {};
    }

    if (points.size() < 3) {
        return {};
    }

    // 默认使用三次样条插补
    return CubicSplineInterpolation(points, 0.5f, config);
}

std::vector<TrajectoryPoint> SplineInterpolator::CubicSplineInterpolation(const std::vector<Point2D>& control_points,
                                                                          float32 tension,
                                                                          const InterpolationConfig& config) {
    if (!ValidateSplineParameters(control_points, SplineType::CUBIC_NATURAL)) {
        return {};
    }

    // 计算三次样条系数
    std::vector<SplineSegment> segments = CalculateCubicSplineCoefficients(control_points, tension, "natural");

    if (segments.empty()) {
        return {};
    }

    return GenerateSplineTrajectory(segments, config);
}

std::vector<TrajectoryPoint> SplineInterpolator::BezierInterpolation(const BezierControlPoints& control_points,
                                                                     const InterpolationConfig& config) {
    if (control_points.control_points.size() < 2) {
        return {};
    }

    return GenerateBezierTrajectory(control_points, config);
}

std::vector<TrajectoryPoint> SplineInterpolator::BSplineInterpolation(const std::vector<Point2D>& control_points,
                                                                      const BSplineKnotVector& knot_vector,
                                                                      const InterpolationConfig& config) {
    if (control_points.size() != knot_vector.num_control_points) {
        return {};
    }

    std::vector<TrajectoryPoint> trajectory;
    float32 t_start = knot_vector.knots[knot_vector.degree];
    float32 t_end = knot_vector.knots[knot_vector.knots.size() - knot_vector.degree - 1];

    int32 num_points = static_cast<int32>((t_end - t_start) / config.time_step);

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = t_start + (t_end - t_start) * static_cast<float32>(i) / static_cast<float32>(num_points);

        Point2D point(0.0f, 0.0f);
        float32 weight_sum = 0.0f;

        for (size_t j = 0; j < control_points.size(); ++j) {
            float32 basis =
                m_bspline_calculator->BSplineBasis(static_cast<int32>(j), knot_vector.degree, t, knot_vector.knots);
            point.x += control_points[j].x * basis;
            point.y += control_points[j].y * basis;
            weight_sum += basis;
        }

        if (weight_sum > 1e-6f) {
            point.x /= weight_sum;
            point.y /= weight_sum;
        }

        TrajectoryPoint trajectory_point;
        trajectory_point.position = point;
        trajectory_point.timestamp = i * config.time_step;
        trajectory_point.sequence_id = static_cast<uint32>(i);
        trajectory_point.velocity = config.max_velocity;

        trajectory.push_back(trajectory_point);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> SplineInterpolator::NURBSInterpolation(const std::vector<Point2D>& control_points,
                                                                    const std::vector<float32>& weights,
                                                                    const std::vector<float32>& knot_vector,
                                                                    int32 degree,
                                                                    const InterpolationConfig& config) {
    if (control_points.size() != weights.size()) {
        return {};
    }

    std::vector<TrajectoryPoint> trajectory;
    float32 t_start = knot_vector[degree];
    float32 t_end = knot_vector[knot_vector.size() - degree - 1];

    int32 num_points = static_cast<int32>((t_end - t_start) / config.time_step);

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = t_start + (t_end - t_start) * static_cast<float32>(i) / static_cast<float32>(num_points);

        Point2D point(0.0f, 0.0f);
        float32 weight_sum = 0.0f;

        for (size_t j = 0; j < control_points.size(); ++j) {
            float32 basis = m_bspline_calculator->BSplineBasis(static_cast<int32>(j), degree, t, knot_vector);
            float32 weighted_basis = basis * weights[j];

            point.x += control_points[j].x * weighted_basis;
            point.y += control_points[j].y * weighted_basis;
            weight_sum += weighted_basis;
        }

        if (weight_sum > 1e-6f) {
            point.x /= weight_sum;
            point.y /= weight_sum;
        }

        TrajectoryPoint trajectory_point;
        trajectory_point.position = point;
        trajectory_point.timestamp = i * config.time_step;
        trajectory_point.sequence_id = static_cast<uint32>(i);
        trajectory_point.velocity = config.max_velocity;

        trajectory.push_back(trajectory_point);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> SplineInterpolator::AdaptiveSplineInterpolation(const std::vector<Point2D>& points,
                                                                             float32 max_error,
                                                                             const InterpolationConfig& config) {
    // 首先生成基础样条
    std::vector<SplineSegment> segments = CalculateCubicSplineCoefficients(points, 0.5f, "natural");

    if (segments.empty()) {
        return {};
    }

    // 自适应步长控制
    std::vector<float32> adaptive_steps = AdaptiveStepControl(segments, max_error, config.time_step);

    std::vector<TrajectoryPoint> trajectory;
    float32 current_x = segments[0].x_start;

    for (size_t i = 0; i < adaptive_steps.size(); ++i) {
        // 找到对应的样条段
        size_t segment_idx = 0;
        for (size_t j = 0; j < segments.size(); ++j) {
            if (current_x >= segments[j].x_start && current_x <= segments[j].x_end) {
                segment_idx = j;
                break;
            }
        }

        const auto& segment = segments[segment_idx];
        float32 local_x = current_x - segment.x_start;

        // 计算样条值
        float32 y =
            segment.a + segment.b * local_x + segment.c * local_x * local_x + segment.d * local_x * local_x * local_x;

        Point2D point(current_x, y);

        TrajectoryPoint trajectory_point;
        trajectory_point.position = point;
        trajectory_point.timestamp = i * config.time_step;
        trajectory_point.sequence_id = static_cast<uint32>(i);

        // 计算速度（基于一阶导数）
        float32 derivative = segment.b + 2.0f * segment.c * local_x + 3.0f * segment.d * local_x * local_x;
        trajectory_point.velocity = std::sqrt(1.0f + derivative * derivative) * config.max_velocity;

        trajectory.push_back(trajectory_point);

        current_x += adaptive_steps[i];
    }

    return trajectory;
}

std::vector<SplineInterpolator::SplineSegment> SplineInterpolator::CalculateCubicSplineCoefficients(
    const std::vector<Point2D>& points, float32 tension, const std::string& boundary_type) const {
    size_t n = points.size();
    if (n < 3) {
        return {};
    }

    std::vector<SplineSegment> segments(n - 1);

    // 准备数据
    std::vector<float32> x(n), y(n);
    for (size_t i = 0; i < n; ++i) {
        x[i] = points[i].x;
        y[i] = points[i].y;
    }

    // 计算步长
    std::vector<float32> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = x[i + 1] - x[i];
    }

    // 构建三对角矩阵系统
    std::vector<float32> a(n - 2), b(n - 2), c(n - 2), d(n - 2);

    for (size_t i = 1; i < n - 1; ++i) {
        size_t idx = i - 1;
        a[idx] = h[i - 1];
        b[idx] = 2.0f * (h[i - 1] + h[i]);
        c[idx] = h[i];
        d[idx] = 6.0f * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);
    }

    // 应用张力系数
    float32 tension_factor = (1.0f - tension);
    for (size_t i = 0; i < a.size(); ++i) {
        d[i] *= tension_factor;
    }

    // 求解三对角系统
    std::vector<float32> m = m_math_utils->ThomasAlgorithm(a, b, c, d);

    // 添加边界条件
    std::vector<float32> second_derivatives(n);
    if (boundary_type == "natural") {
        second_derivatives[0] = 0.0f;
        second_derivatives[n - 1] = 0.0f;
    } else if (boundary_type == "clamped") {
        // 固定边界条件：二阶导数为0
        second_derivatives[0] = 0.0f;
        second_derivatives[n - 1] = 0.0f;
    }

    for (size_t i = 1; i < n - 1; ++i) {
        second_derivatives[i] = m[i - 1] * tension_factor;
    }

    // 计算每段的系数
    for (size_t i = 0; i < n - 1; ++i) {
        float32 hi = h[i];
        segments[i].a = y[i];
        segments[i].b = (y[i + 1] - y[i]) / hi - hi * (2.0f * second_derivatives[i] + second_derivatives[i + 1]) / 6.0f;
        segments[i].c = second_derivatives[i] / 2.0f;
        segments[i].d = (second_derivatives[i + 1] - second_derivatives[i]) / (6.0f * hi);
        segments[i].x_start = x[i];
        segments[i].x_end = x[i + 1];
    }

    return segments;
}

Point2D SplineInterpolator::DeCasteljau(const std::vector<Point2D>& control_points, float32 t) const {
    return m_bezier_calculator->DeCasteljau(control_points, t);
}

float32 SplineInterpolator::BSplineBasis(int32 i, int32 p, float32 t, const std::vector<float32>& knots) const {
    return m_bspline_calculator->BSplineBasis(i, p, t, knots);
}

float32 SplineInterpolator::CalculateSplineLength(const std::vector<SplineSegment>& segments, int32 num_samples) const {
    return m_math_utils->CalculateSplineLength(segments, num_samples);
}

bool SplineInterpolator::ValidateSplineParameters(const std::vector<Point2D>& control_points,
                                                  SplineType spline_type) const {
    if (control_points.size() < 2) {
        return false;
    }

    // 检查控制点是否按x坐标排序
    for (size_t i = 1; i < control_points.size(); ++i) {
        if (control_points[i].x <= control_points[i - 1].x) {
            return false;
        }
    }

    return true;
}

std::vector<TrajectoryPoint> SplineInterpolator::GenerateSplineTrajectory(const std::vector<SplineSegment>& segments,
                                                                          const InterpolationConfig& config) const {
    std::vector<TrajectoryPoint> trajectory;

    float32 total_length = CalculateSplineLength(segments);
    float32 total_time = total_length / config.max_velocity;

    // 计算总点数
    int32 num_points = static_cast<int32>(total_time / config.time_step);

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(num_points);
        float32 target_length = t * total_length;

        // 找到对应的样条段和参数
        float32 current_length = 0.0f;
        size_t segment_idx = 0;
        float32 local_t = 0.0f;

        for (size_t j = 0; j < segments.size(); ++j) {
            float32 segment_length = CalculateSplineLength({segments[j]}, 100);

            if (current_length + segment_length >= target_length) {
                segment_idx = j;
                local_t = (target_length - current_length) / segment_length;
                break;
            }
            current_length += segment_length;
        }

        const auto& segment = segments[segment_idx];
        float32 x = segment.x_start + local_t * (segment.x_end - segment.x_start);
        float32 local_x = x - segment.x_start;
        float32 y =
            segment.a + segment.b * local_x + segment.c * local_x * local_x + segment.d * local_x * local_x * local_x;

        Point2D current_point(x, y);

        TrajectoryPoint point;
        point.position = current_point;
        point.timestamp = t * total_time;
        point.sequence_id = static_cast<uint32>(i);

        // 计算速度和加速度
        float32 first_derivative = segment.b + 2.0f * segment.c * local_x + 3.0f * segment.d * local_x * local_x;
        float32 second_derivative = 2.0f * segment.c + 6.0f * segment.d * local_x;

        point.velocity = std::sqrt(1.0f + first_derivative * first_derivative) * config.max_velocity;
        point.acceleration = std::abs(second_derivative) / std::pow(1.0f + first_derivative * first_derivative, 1.5f) *
                             config.max_velocity * config.max_velocity;

        trajectory.push_back(point);
    }

    return trajectory;
}

std::vector<TrajectoryPoint> SplineInterpolator::GenerateBezierTrajectory(const BezierControlPoints& control_points,
                                                                          const InterpolationConfig& config) const {
    return m_bezier_calculator->GenerateBezierTrajectory(control_points, config);
}

float32 SplineInterpolator::CalculateSplineDerivative(const std::vector<SplineSegment>& segments, float32 x) const {
    return m_math_utils->CalculateSplineDerivative(segments, x);
}

float32 SplineInterpolator::CalculateSplineCurvature(const std::vector<SplineSegment>& segments, float32 x) const {
    return m_math_utils->CalculateSplineCurvature(segments, x);
}

std::vector<float32> SplineInterpolator::AdaptiveStepControl(const std::vector<SplineSegment>& segments,
                                                             float32 max_error,
                                                             float32 initial_step) const {
    return m_math_utils->AdaptiveStepControl(segments, max_error, initial_step);
}

std::vector<float32> SplineInterpolator::CalculateSplineTriggerPositions(const std::vector<SplineSegment>& segments,
                                                                         const InterpolationConfig& config) const {
    std::vector<float32> trigger_positions;

    float32 total_length = CalculateSplineLength(segments);
    float32 trigger_spacing = 10.0f;  // 每10mm一个触发点

    int32 num_triggers = static_cast<int32>(std::floor(total_length / trigger_spacing));
    for (int32 i = 1; i < num_triggers; ++i) {
        float32 trigger_position = static_cast<float32>(i) / static_cast<float32>(num_triggers);
        trigger_positions.push_back(trigger_position);
    }

    return trigger_positions;
}

std::vector<Point2D> SplineInterpolator::ParametericToCartesian(const std::vector<Point2D>& control_points,
                                                                const std::vector<float32>& parameter_values,
                                                                SplineType spline_type) const {
    std::vector<Point2D> cartesian_points;

    switch (spline_type) {
        case SplineType::BEZIER: {
            for (float32 t : parameter_values) {
                cartesian_points.push_back(DeCasteljau(control_points, t));
            }
            break;
        }
        default:
            break;
    }

    return cartesian_points;
}

}  // namespace Siligen::Domain::Motion

