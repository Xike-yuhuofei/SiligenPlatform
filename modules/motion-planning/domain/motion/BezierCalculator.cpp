/**
 * @file BezierCalculator.cpp
 * @brief Bezier曲线计算器实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#include "BezierCalculator.h"

namespace Siligen::Domain::Motion {

Point2D BezierCalculator::DeCasteljau(const std::vector<Point2D>& control_points, float32 t) const {
    if (control_points.empty()) {
        return Point2D(0.0f, 0.0f);
    }

    std::vector<Point2D> temp_points = control_points;

    for (size_t k = 1; k < control_points.size(); ++k) {
        for (size_t i = 0; i < control_points.size() - k; ++i) {
            temp_points[i].x = (1.0f - t) * temp_points[i].x + t * temp_points[i + 1].x;
            temp_points[i].y = (1.0f - t) * temp_points[i].y + t * temp_points[i + 1].y;
        }
    }

    return temp_points[0];
}

std::vector<TrajectoryPoint> BezierCalculator::GenerateBezierTrajectory(
    const SplineInterpolator::BezierControlPoints& control_points, const InterpolationConfig& config) const {
    std::vector<TrajectoryPoint> trajectory;

    int32 num_points = static_cast<int32>(1.0f / config.time_step);

    for (int32 i = 0; i <= num_points; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(num_points);
        Point2D current_point = DeCasteljau(control_points.control_points, t);

        TrajectoryPoint point;
        point.position = current_point;
        point.timestamp = t;
        point.sequence_id = static_cast<uint32>(i);
        point.velocity = config.max_velocity;

        trajectory.push_back(point);
    }

    return trajectory;
}

}  // namespace Siligen::Domain::Motion
