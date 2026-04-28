#pragma once

#include "process_path/contracts/Path.h"
#include "process_path/contracts/Primitive.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Siligen::ProcessPath::Contracts {

constexpr float32 kGeometryEpsilon = 1e-6f;
constexpr float32 kEllipseEpsilon = 1e-6f;

struct GeometryTolerance {
    // Linear tolerance is an absolute millimeter tolerance in canonical M6 coordinates.
    float32 linear_mm = kGeometryEpsilon;
    // Angular tolerance is an absolute degree tolerance.
    float32 angular_deg = 1e-3f;
};

inline float32 Clamp(const float32 value, const float32 lower, const float32 upper) {
    return std::min(std::max(value, lower), upper);
}

inline float32 NormalizeLinearTolerance(const float32 tolerance_mm) {
    return tolerance_mm > kGeometryEpsilon ? tolerance_mm : kGeometryEpsilon;
}

inline GeometryTolerance NormalizeTolerance(const GeometryTolerance tolerance) {
    return GeometryTolerance{
        NormalizeLinearTolerance(tolerance.linear_mm),
        tolerance.angular_deg > kGeometryEpsilon ? tolerance.angular_deg : 1e-3f};
}

inline bool IsFinite(const float32 value) {
    return std::isfinite(value);
}

inline bool IsFinite(const Point2D& point) {
    return IsFinite(point.x) && IsFinite(point.y);
}

inline bool IsZeroLength(const float32 length, const float32 tolerance_mm = kGeometryEpsilon) {
    return length <= NormalizeLinearTolerance(tolerance_mm);
}

inline bool AreClose(const float32 lhs,
                     const float32 rhs,
                     const float32 tolerance = kGeometryEpsilon) {
    return std::abs(lhs - rhs) <= NormalizeLinearTolerance(tolerance);
}

inline bool ArePointsClose(const Point2D& lhs,
                           const Point2D& rhs,
                           const float32 tolerance_mm = kGeometryEpsilon) {
    return lhs.DistanceTo(rhs) <= NormalizeLinearTolerance(tolerance_mm);
}

inline bool IsDegenerateLine(const LinePrimitive& line,
                             const float32 tolerance_mm = kGeometryEpsilon) {
    return IsZeroLength(line.start.DistanceTo(line.end), tolerance_mm);
}

inline bool IsDegenerateArc(const ArcPrimitive& arc,
                            const float32 tolerance_mm = kGeometryEpsilon) {
    return arc.radius <= NormalizeLinearTolerance(tolerance_mm) || !IsFinite(arc.center);
}

inline float32 NormalizeAngle(const float32 angle_deg) {
    float32 mod = std::fmod(angle_deg, 360.0f);
    if (mod < 0.0f) {
        mod += 360.0f;
    }
    return mod;
}

inline float32 NormalizeSweep(const float32 diff) {
    float32 mod = std::fmod(diff, 360.0f);
    if (mod < 0.0f) {
        mod += 360.0f;
    }
    if (std::abs(mod) <= kGeometryEpsilon && std::abs(diff) > kGeometryEpsilon) {
        return 360.0f;
    }
    return mod;
}

inline float32 ComputeArcSweep(const float32 start_angle, const float32 end_angle, const bool clockwise) {
    const float32 diff = clockwise ? (start_angle - end_angle) : (end_angle - start_angle);
    return NormalizeSweep(diff);
}

inline float32 ComputeArcLength(const ArcPrimitive& arc) {
    if (arc.radius <= kGeometryEpsilon) {
        return 0.0f;
    }
    const float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    return arc.radius * sweep * Siligen::Shared::Types::kDegToRad;
}

inline Point2D ArcPoint(const ArcPrimitive& arc, const float32 angle_deg) {
    const float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    return Point2D(
        arc.center.x + arc.radius * std::cos(angle_rad),
        arc.center.y + arc.radius * std::sin(angle_rad));
}

inline Point2D ArcPoint(const Point2D& center, const float32 radius, const float32 angle_deg) {
    const float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    return Point2D(center.x + radius * std::cos(angle_rad), center.y + radius * std::sin(angle_rad));
}

inline Point2D ArcTangent(const ArcPrimitive& arc, const float32 angle_deg) {
    const float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    if (arc.clockwise) {
        return Point2D(std::sin(angle_rad), -std::cos(angle_rad));
    }
    return Point2D(-std::sin(angle_rad), std::cos(angle_rad));
}

inline Point2D ArcTangentAt(const float32 angle_deg, const bool clockwise) {
    const float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    if (clockwise) {
        return Point2D(std::sin(angle_rad), -std::cos(angle_rad));
    }
    return Point2D(-std::sin(angle_rad), std::cos(angle_rad));
}

inline bool IsAngleOnArc(const ArcPrimitive& arc,
                         const float32 angle_deg,
                         const float32 angular_tolerance_deg = 1e-3f) {
    const float32 total = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    const float32 sweep = ComputeArcSweep(arc.start_angle_deg, angle_deg, arc.clockwise);
    const float32 tolerance = angular_tolerance_deg > kGeometryEpsilon ? angular_tolerance_deg : 1e-3f;
    return sweep <= total + tolerance;
}

inline bool IsPointOnSegment(const Point2D& point,
                             const Point2D& start,
                             const Point2D& end,
                             const float32 tolerance_mm = kGeometryEpsilon) {
    const Point2D segment = end - start;
    const float32 len2 = segment.Dot(segment);
    const float32 tolerance = NormalizeLinearTolerance(tolerance_mm);
    if (len2 <= tolerance * tolerance) {
        return ArePointsClose(point, start, tolerance);
    }
    const float32 t = (point - start).Dot(segment) / len2;
    const Point2D projection = start + segment * Clamp(t, 0.0f, 1.0f);
    return ArePointsClose(point, projection, tolerance);
}

inline Point2D LineDirection(const LinePrimitive& line) {
    const Point2D direction = line.end - line.start;
    const float32 length = direction.Length();
    if (length <= kGeometryEpsilon) {
        return Point2D(0.0f, 0.0f);
    }
    return direction / length;
}

inline Point2D SegmentStart(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return segment.line.start;
    }
    if (segment.type == SegmentType::Arc) {
        return ArcPoint(segment.arc, segment.arc.start_angle_deg);
    }
    if (!segment.spline.control_points.empty()) {
        return segment.spline.control_points.front();
    }
    return {};
}

inline Point2D SegmentEnd(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return segment.line.end;
    }
    if (segment.type == SegmentType::Arc) {
        return ArcPoint(segment.arc, segment.arc.end_angle_deg);
    }
    if (!segment.spline.control_points.empty()) {
        return segment.spline.control_points.back();
    }
    return {};
}

inline Point2D EllipsePoint(const EllipsePrimitive& ellipse, const float32 param) {
    const float32 major = ellipse.major_axis.Length();
    const float32 minor = major * ellipse.ratio;
    if (major <= kEllipseEpsilon || minor <= kEllipseEpsilon) {
        return ellipse.center;
    }
    const float32 theta = std::atan2(ellipse.major_axis.y, ellipse.major_axis.x);
    const float32 cos_param = std::cos(param);
    const float32 sin_param = std::sin(param);
    const float32 x = major * cos_param;
    const float32 y = minor * sin_param;
    const float32 cos_theta = std::cos(theta);
    const float32 sin_theta = std::sin(theta);
    const float32 rotated_x = x * cos_theta - y * sin_theta;
    const float32 rotated_y = x * sin_theta + y * cos_theta;
    return Point2D(ellipse.center.x + rotated_x, ellipse.center.y + rotated_y);
}

}  // namespace Siligen::ProcessPath::Contracts
