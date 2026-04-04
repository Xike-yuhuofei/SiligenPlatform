#include "CurveFlatteningService.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::SegmentType;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kDefaultFlattenStepMm = 1.0f;

bool IsFiniteFloat(float32 value) {
    return std::isfinite(value);
}

bool IsFinitePoint(const Point2D& point) {
    return IsFiniteFloat(point.x) && IsFiniteFloat(point.y);
}

void AppendPoint(std::vector<Point2D>& points, const Point2D& point) {
    if (points.empty() || points.back().DistanceTo(point) > kEpsilon) {
        points.push_back(point);
    }
}

Result<FlattenedCurvePath> BuildCumulativeLengths(std::vector<Point2D> points, const char* source) {
    FlattenedCurvePath flattened;
    flattened.points = std::move(points);
    if (flattened.points.size() < 2U) {
        return Result<FlattenedCurvePath>::Failure(
            Error(ErrorCode::INVALID_STATE, "flattened curve path is empty", source));
    }

    flattened.cumulative_lengths_mm.reserve(flattened.points.size());
    flattened.cumulative_lengths_mm.push_back(0.0f);
    for (std::size_t index = 1; index < flattened.points.size(); ++index) {
        if (!IsFinitePoint(flattened.points[index - 1]) || !IsFinitePoint(flattened.points[index])) {
            return Result<FlattenedCurvePath>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "flattened curve path contains non-finite coordinates", source));
        }
        flattened.total_length_mm += flattened.points[index - 1].DistanceTo(flattened.points[index]);
        flattened.cumulative_lengths_mm.push_back(flattened.total_length_mm);
    }
    if (!IsFiniteFloat(flattened.total_length_mm) || flattened.total_length_mm <= kEpsilon) {
        return Result<FlattenedCurvePath>::Failure(
            Error(ErrorCode::INVALID_STATE, "flattened curve path is degenerate", source));
    }
    return Result<FlattenedCurvePath>::Success(std::move(flattened));
}

float32 ResolveArcStep(float32 radius_mm, float32 max_error_mm, float32 max_step_mm) {
    float32 step_mm = 0.0f;
    if (max_error_mm > kEpsilon && radius_mm > kEpsilon) {
        const float32 error_step_mm = std::sqrt(std::max(8.0f * radius_mm * max_error_mm, kEpsilon));
        if (std::isfinite(error_step_mm) && error_step_mm > kEpsilon) {
            step_mm = error_step_mm;
        }
    }
    if (max_step_mm > kEpsilon) {
        step_mm = step_mm > kEpsilon ? std::min(step_mm, max_step_mm) : max_step_mm;
    }
    if (step_mm <= kEpsilon) {
        step_mm = kDefaultFlattenStepMm;
    }
    return step_mm;
}

float32 ClampValue(float32 value, float32 min_value, float32 max_value) {
    return std::max(min_value, std::min(max_value, value));
}

float32 ComputeCircumradius(const Point2D& prev, const Point2D& curr, const Point2D& next) {
    const float32 a = prev.DistanceTo(curr);
    const float32 b = curr.DistanceTo(next);
    const float32 c = prev.DistanceTo(next);
    if (a <= kEpsilon || b <= kEpsilon || c <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }

    const Point2D v0 = curr - prev;
    const Point2D v1 = next - prev;
    const float32 area2 = std::abs(v0.Cross(v1));
    if (area2 <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }

    const float32 radius = (a * b * c) / (2.0f * area2);
    if (!std::isfinite(radius) || radius <= kEpsilon) {
        return std::numeric_limits<float32>::infinity();
    }
    return radius;
}

float32 ComputeAdaptiveSplineStep(const std::vector<Point2D>& points, std::size_t index, float32 base_step_mm) {
    if (base_step_mm <= kEpsilon) {
        return 0.0f;
    }
    if (index == 0U || index + 1U >= points.size()) {
        return base_step_mm;
    }

    const auto& prev = points[index - 1U];
    const auto& curr = points[index];
    const auto& next = points[index + 1U];
    const float32 len1 = prev.DistanceTo(curr);
    const float32 len2 = curr.DistanceTo(next);
    if (len1 <= kEpsilon || len2 <= kEpsilon) {
        return base_step_mm;
    }

    const float32 dot = ClampValue((curr - prev).Dot(next - curr) / (len1 * len2), -1.0f, 1.0f);
    const float32 angle = std::acos(dot);
    const float32 ratio = angle / 3.14159265359f;
    const float32 scale = 1.0f - 0.6f * ratio;
    return std::max(base_step_mm * 0.2f, base_step_mm * scale);
}

float32 ComputeErrorBoundSplineStep(const std::vector<Point2D>& points, std::size_t index, float32 max_error_mm) {
    if (max_error_mm <= kEpsilon || points.size() < 3U) {
        return 0.0f;
    }

    float32 radius = std::numeric_limits<float32>::infinity();
    if (index > 0U && index + 1U < points.size()) {
        radius = std::min(radius, ComputeCircumradius(points[index - 1U], points[index], points[index + 1U]));
    }
    if (index + 2U < points.size()) {
        radius = std::min(radius, ComputeCircumradius(points[index], points[index + 1U], points[index + 2U]));
    }
    if (!std::isfinite(radius)) {
        return 0.0f;
    }

    const float32 step_mm = std::sqrt(std::max(8.0f * radius * max_error_mm, kEpsilon));
    return std::isfinite(step_mm) ? step_mm : 0.0f;
}

void AppendAdaptiveLineSegments(
    const Point2D& start,
    const Point2D& end,
    float32 step_mm,
    std::vector<Point2D>& points) {
    const float32 length_mm = start.DistanceTo(end);
    if (length_mm <= kEpsilon) {
        return;
    }
    if (step_mm <= kEpsilon || length_mm <= step_mm) {
        AppendPoint(points, start);
        AppendPoint(points, end);
        return;
    }

    int slices = static_cast<int>(std::ceil(length_mm / step_mm));
    slices = std::max(1, slices);
    const Point2D delta = end - start;
    for (int index = 0; index <= slices; ++index) {
        const float32 ratio = static_cast<float32>(index) / static_cast<float32>(slices);
        AppendPoint(points, start + delta * ratio);
    }
}

std::vector<Point2D> DeduplicateSplinePoints(const std::vector<Point2D>& control_points, bool closed) {
    std::vector<Point2D> points;
    points.reserve(control_points.size() + (closed ? 1U : 0U));
    for (const auto& point : control_points) {
        AppendPoint(points, point);
    }
    if (closed && !points.empty() && points.front().DistanceTo(points.back()) > kEpsilon) {
        points.push_back(points.front());
    }
    return points;
}

Result<FlattenedCurvePath> BuildSplinePath(
    const Segment& segment,
    float32 max_error_mm,
    float32 max_step_mm) {
    if (segment.spline.control_points.empty()) {
        return Result<FlattenedCurvePath>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "spline control points are empty", "CurveFlatteningService"));
    }
    for (const auto& point : segment.spline.control_points) {
        if (!IsFinitePoint(point)) {
            return Result<FlattenedCurvePath>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "spline contains non-finite control point", "CurveFlatteningService"));
        }
    }

    const auto normalized_points = DeduplicateSplinePoints(segment.spline.control_points, segment.spline.closed);
    if (normalized_points.size() < 2U) {
        return Result<FlattenedCurvePath>::Failure(
            Error(ErrorCode::INVALID_STATE, "spline flattened path unavailable", "CurveFlatteningService"));
    }

    std::vector<Point2D> points;
    points.reserve(normalized_points.size() * 4U);
    const float32 base_step_mm = max_step_mm > kEpsilon ? max_step_mm : kDefaultFlattenStepMm;
    for (std::size_t index = 0; index + 1U < normalized_points.size(); ++index) {
        float32 step_mm = ComputeAdaptiveSplineStep(normalized_points, index, base_step_mm);
        const float32 error_step_mm = ComputeErrorBoundSplineStep(normalized_points, index, max_error_mm);
        if (error_step_mm > kEpsilon) {
            step_mm = step_mm > kEpsilon ? std::min(step_mm, error_step_mm) : error_step_mm;
        }
        if (step_mm <= kEpsilon) {
            step_mm = base_step_mm;
        }
        AppendAdaptiveLineSegments(
            normalized_points[index],
            normalized_points[index + 1U],
            step_mm,
            points);
    }
    if (points.size() < 2U) {
        return Result<FlattenedCurvePath>::Failure(
            Error(ErrorCode::INVALID_STATE, "spline flattened path unavailable", "CurveFlatteningService"));
    }
    return BuildCumulativeLengths(std::move(points), "CurveFlatteningService");
}

}  // namespace

Result<FlattenedCurvePath> CurveFlatteningService::Flatten(
    const Segment& segment,
    float32 max_error_mm,
    float32 max_step_mm) const {
    std::vector<Point2D> points;
    switch (segment.type) {
        case SegmentType::Line: {
            if (!IsFinitePoint(segment.line.start) || !IsFinitePoint(segment.line.end)) {
                return Result<FlattenedCurvePath>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "line contains non-finite coordinates", "CurveFlatteningService"));
            }
            if (segment.line.start.DistanceTo(segment.line.end) <= kEpsilon) {
                return Result<FlattenedCurvePath>::Failure(
                    Error(ErrorCode::INVALID_STATE, "line segment is degenerate", "CurveFlatteningService"));
            }
            AppendPoint(points, segment.line.start);
            AppendPoint(points, segment.line.end);
            break;
        }
        case SegmentType::Arc: {
            if (!IsFinitePoint(segment.arc.center) ||
                !IsFiniteFloat(segment.arc.radius) ||
                !IsFiniteFloat(segment.arc.start_angle_deg) ||
                !IsFiniteFloat(segment.arc.end_angle_deg) ||
                segment.arc.radius <= kEpsilon) {
                return Result<FlattenedCurvePath>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "arc geometry is invalid", "CurveFlatteningService"));
            }

            const float32 arc_length = ComputeArcLength(segment.arc);
            if (arc_length <= kEpsilon) {
                return Result<FlattenedCurvePath>::Failure(
                    Error(ErrorCode::INVALID_STATE, "arc segment is degenerate", "CurveFlatteningService"));
            }

            const float32 safe_step = ResolveArcStep(segment.arc.radius, max_error_mm, max_step_mm);
            int slices = static_cast<int>(std::ceil(arc_length / safe_step));
            slices = std::max(1, slices);
            const float32 sweep = ComputeArcSweep(
                segment.arc.start_angle_deg,
                segment.arc.end_angle_deg,
                segment.arc.clockwise);
            const float32 direction = segment.arc.clockwise ? -1.0f : 1.0f;
            for (int index = 0; index <= slices; ++index) {
                const float32 ratio = static_cast<float32>(index) / static_cast<float32>(slices);
                const float32 angle = segment.arc.start_angle_deg + direction * sweep * ratio;
                AppendPoint(points, ArcPoint(segment.arc, angle));
            }
            break;
        }
        case SegmentType::Spline:
            return BuildSplinePath(segment, max_error_mm, max_step_mm);
        default:
            return Result<FlattenedCurvePath>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "unsupported segment type", "CurveFlatteningService"));
    }

    return BuildCumulativeLengths(std::move(points), "CurveFlatteningService");
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
