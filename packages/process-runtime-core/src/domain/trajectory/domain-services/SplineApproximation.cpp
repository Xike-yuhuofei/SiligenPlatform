#include "SplineApproximation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kDefaultSplineStepMm = 1.0f;

float32 ClampValue(float32 value, float32 min_value, float32 max_value) {
    return std::max(min_value, std::min(max_value, value));
}

float32 ComputeCircumradius(const Point2D& prev,
                            const Point2D& curr,
                            const Point2D& next) {
    float32 a = prev.DistanceTo(curr);
    float32 b = curr.DistanceTo(next);
    float32 c = prev.DistanceTo(next);
    if (a <= kEpsilon || b <= kEpsilon || c <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }

    Point2D v0 = curr - prev;
    Point2D v1 = next - prev;
    float32 area2 = std::abs(v0.Cross(v1));
    if (area2 <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }
    float32 radius = (a * b * c) / (2.0f * area2);
    if (!std::isfinite(radius) || radius <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }
    return radius;
}

float32 ComputeAdaptiveSplineStep(const std::vector<Point2D>& points,
                                  size_t index,
                                  float32 base_step) {
    if (base_step <= kEpsilon) {
        return 0.0f;
    }
    if (index == 0 || index + 1 >= points.size()) {
        return base_step;
    }

    const auto& prev = points[index - 1];
    const auto& curr = points[index];
    const auto& next = points[index + 1];
    float32 len1 = prev.DistanceTo(curr);
    float32 len2 = curr.DistanceTo(next);
    if (len1 <= kEpsilon || len2 <= kEpsilon) {
        return base_step;
    }

    float32 v1x = curr.x - prev.x;
    float32 v1y = curr.y - prev.y;
    float32 v2x = next.x - curr.x;
    float32 v2y = next.y - curr.y;
    float32 dot = (v1x * v2x + v1y * v2y) / (len1 * len2);
    dot = ClampValue(dot, -1.0f, 1.0f);
    float32 angle = std::acos(dot);
    float32 ratio = angle / 3.14159265359f;
    float32 scale = 1.0f - 0.6f * ratio;
    float32 min_step = base_step * 0.2f;
    return std::max(min_step, base_step * scale);
}

float32 ComputeErrorBoundSplineStep(const std::vector<Point2D>& points,
                                    size_t index,
                                    float32 max_error_mm) {
    if (max_error_mm <= kEpsilon || points.size() < 3) {
        return 0.0f;
    }

    float32 radius = std::numeric_limits<float32>::infinity();
    if (index > 0 && index + 1 < points.size()) {
        radius = std::min(radius,
                          ComputeCircumradius(points[index - 1], points[index], points[index + 1]));
    }
    if (index + 2 < points.size()) {
        radius = std::min(radius,
                          ComputeCircumradius(points[index], points[index + 1], points[index + 2]));
    }
    if (!std::isfinite(radius)) {
        return 0.0f;
    }

    float32 step = std::sqrt(8.0f * radius * max_error_mm);
    if (!std::isfinite(step) || step <= kEpsilon) {
        return 0.0f;
    }
    return step;
}

float32 ComputeSplineRadius(const std::vector<Point2D>& points, size_t index) {
    float32 radius = std::numeric_limits<float32>::infinity();
    if (index > 0 && index + 1 < points.size()) {
        radius = std::min(radius,
                          ComputeCircumradius(points[index - 1], points[index], points[index + 1]));
    }
    if (index + 2 < points.size()) {
        radius = std::min(radius,
                          ComputeCircumradius(points[index], points[index + 1], points[index + 2]));
    }
    if (!std::isfinite(radius)) {
        return 0.0f;
    }
    return radius;
}

void AppendLineSegment(const Point2D& start,
                       const Point2D& end,
                       float32 curvature_radius,
                       float32 min_segment_mm,
                       std::vector<Segment>& segments) {
    Segment seg;
    seg.type = Siligen::Domain::Trajectory::ValueObjects::SegmentType::Line;
    seg.line.start = start;
    seg.line.end = end;
    seg.length = start.DistanceTo(end);
    if (seg.length <= kEpsilon || (min_segment_mm > kEpsilon && seg.length < min_segment_mm)) {
        return;
    }
    seg.curvature_radius = curvature_radius;
    seg.is_spline_approx = true;
    segments.push_back(seg);
}

void AppendAdaptiveLineSegments(const Point2D& start,
                                const Point2D& end,
                                float32 step_mm,
                                float32 curvature_radius,
                                float32 min_segment_mm,
                                std::vector<Segment>& segments) {
    float32 length = start.DistanceTo(end);
    if (length <= kEpsilon) {
        return;
    }
    if (step_mm <= kEpsilon || length <= step_mm) {
        AppendLineSegment(start, end, curvature_radius, min_segment_mm, segments);
        return;
    }

    int slices = static_cast<int>(std::ceil(length / step_mm));
    if (slices < 1) {
        slices = 1;
    }
    Point2D delta = end - start;
    Point2D prev = start;
    for (int i = 1; i <= slices; ++i) {
        float32 t = static_cast<float32>(i) / static_cast<float32>(slices);
        Point2D next = start + delta * t;
        AppendLineSegment(prev, next, curvature_radius, min_segment_mm, segments);
        prev = next;
    }
}

}  // namespace

std::vector<Segment> SplineApproximation::Approximate(const SplinePrimitive& spline,
                                                      const SplineApproximationConfig& config) const {
    std::vector<Segment> segments;
    const auto& points = spline.control_points;
    if (points.size() < 2) {
        return segments;
    }

    float32 base_step = (config.max_step_mm > kEpsilon) ? config.max_step_mm : kDefaultSplineStepMm;

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        float32 step = 0.0f;
        if (config.max_step_mm > kEpsilon) {
            step = ComputeAdaptiveSplineStep(points, i, base_step);
        }
        float32 error_step = ComputeErrorBoundSplineStep(points, i, config.max_error_mm);
        if (error_step > kEpsilon) {
            step = (step > kEpsilon) ? std::min(step, error_step) : error_step;
        }

        float32 curvature_radius = ComputeSplineRadius(points, i);
        AppendAdaptiveLineSegments(points[i],
                                   points[i + 1],
                                   step,
                                   curvature_radius,
                                   config.min_segment_mm,
                                   segments);
    }

    return segments;
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
