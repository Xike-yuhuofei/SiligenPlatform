#pragma once

#include "Path.h"
#include "Primitive.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Trajectory::ValueObjects {

constexpr float32 kGeometryEpsilon = 1e-6f;
constexpr float32 kEllipseEpsilon = 1e-6f;

inline float32 Clamp(float32 v, float32 lo, float32 hi) {
    return std::min(std::max(v, lo), hi);
}

inline float32 NormalizeAngle(float32 angle_deg) {
    float32 mod = std::fmod(angle_deg, 360.0f);
    if (mod < 0.0f) {
        mod += 360.0f;
    }
    return mod;
}

inline float32 NormalizeSweep(float32 diff) {
    float32 mod = std::fmod(diff, 360.0f);
    if (mod < 0.0f) {
        mod += 360.0f;
    }
    if (std::abs(mod) <= kGeometryEpsilon && std::abs(diff) > kGeometryEpsilon) {
        return 360.0f;
    }
    return mod;
}

inline float32 ComputeArcSweep(float32 start_angle, float32 end_angle, bool clockwise) {
    float32 diff = clockwise ? (start_angle - end_angle) : (end_angle - start_angle);
    return NormalizeSweep(diff);
}

inline float32 ComputeArcLength(const ArcPrimitive& arc) {
    if (arc.radius <= kGeometryEpsilon) {
        return 0.0f;
    }
    float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    return arc.radius * sweep * Siligen::Shared::Types::kDegToRad;
}

inline Point2D ArcPoint(const ArcPrimitive& arc, float32 angle_deg) {
    float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    return Point2D(arc.center.x + arc.radius * std::cos(angle_rad),
                   arc.center.y + arc.radius * std::sin(angle_rad));
}

inline Point2D ArcPoint(const Point2D& center, float32 radius, float32 angle_deg) {
    float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    return Point2D(center.x + radius * std::cos(angle_rad),
                   center.y + radius * std::sin(angle_rad));
}

inline Point2D ArcTangent(const ArcPrimitive& arc, float32 angle_deg) {
    float32 angle_rad = angle_deg * Siligen::Shared::Types::kDegToRad;
    if (arc.clockwise) {
        return Point2D(std::sin(angle_rad), -std::cos(angle_rad));
    }
    return Point2D(-std::sin(angle_rad), std::cos(angle_rad));
}

inline Point2D LineDirection(const LinePrimitive& line) {
    Point2D dir = line.end - line.start;
    float32 len = dir.Length();
    if (len <= kGeometryEpsilon) {
        return Point2D(0.0f, 0.0f);
    }
    return dir / len;
}

inline Point2D SegmentStart(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return segment.line.start;
    }
    return ArcPoint(segment.arc, segment.arc.start_angle_deg);
}

inline Point2D SegmentEnd(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return segment.line.end;
    }
    return ArcPoint(segment.arc, segment.arc.end_angle_deg);
}

inline Point2D EllipsePoint(const EllipsePrimitive& ellipse, float32 param) {
    float32 a = ellipse.major_axis.Length();
    float32 b = a * ellipse.ratio;
    if (a <= kEllipseEpsilon || b <= kEllipseEpsilon) {
        return ellipse.center;
    }
    float32 theta = std::atan2(ellipse.major_axis.y, ellipse.major_axis.x);
    float32 cos_t = std::cos(param);
    float32 sin_t = std::sin(param);
    float32 x = a * cos_t;
    float32 y = b * sin_t;
    float32 cos_theta = std::cos(theta);
    float32 sin_theta = std::sin(theta);
    float32 xr = x * cos_theta - y * sin_theta;
    float32 yr = x * sin_theta + y * cos_theta;
    return Point2D(ellipse.center.x + xr, ellipse.center.y + yr);
}

}  // namespace Siligen::Domain::Trajectory::ValueObjects
