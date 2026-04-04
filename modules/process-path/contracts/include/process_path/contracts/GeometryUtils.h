#pragma once

#include "process_path/contracts/Path.h"
#include "process_path/contracts/Primitive.h"
#include "shared/types/MathConstants.h"

#include <algorithm>
#include <cmath>

namespace Siligen::ProcessPath::Contracts {

constexpr float32 kGeometryEpsilon = 1e-6f;
constexpr float32 kEllipseEpsilon = 1e-6f;

inline float32 Clamp(const float32 value, const float32 lower, const float32 upper) {
    return std::min(std::max(value, lower), upper);
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
    return ArcPoint(segment.arc, segment.arc.start_angle_deg);
}

inline Point2D SegmentEnd(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return segment.line.end;
    }
    return ArcPoint(segment.arc, segment.arc.end_angle_deg);
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

namespace Siligen::Domain::Trajectory::ValueObjects {

struct ArcPrimitive;
struct EllipsePrimitive;
struct LinePrimitive;
struct Segment;

using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::Point2D;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ArcTangent;
using Siligen::ProcessPath::Contracts::Clamp;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::EllipsePoint;
using Siligen::ProcessPath::Contracts::LineDirection;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::NormalizeSweep;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;

inline float32 ComputeArcLength(const ArcPrimitive& arc) {
    return Siligen::ProcessPath::Contracts::ComputeArcLength(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::ArcPrimitive&>(arc));
}

inline Point2D ArcPoint(const ArcPrimitive& arc, float32 angle_deg) {
    return Siligen::ProcessPath::Contracts::ArcPoint(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::ArcPrimitive&>(arc), angle_deg);
}

inline Point2D ArcTangent(const ArcPrimitive& arc, float32 angle_deg) {
    return Siligen::ProcessPath::Contracts::ArcTangent(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::ArcPrimitive&>(arc), angle_deg);
}

inline Point2D LineDirection(const LinePrimitive& line) {
    return Siligen::ProcessPath::Contracts::LineDirection(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::LinePrimitive&>(line));
}

inline Point2D SegmentStart(const Segment& segment) {
    return Siligen::ProcessPath::Contracts::SegmentStart(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::Segment&>(segment));
}

inline Point2D SegmentEnd(const Segment& segment) {
    return Siligen::ProcessPath::Contracts::SegmentEnd(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::Segment&>(segment));
}

inline Point2D EllipsePoint(const EllipsePrimitive& ellipse, float32 param) {
    return Siligen::ProcessPath::Contracts::EllipsePoint(
        reinterpret_cast<const Siligen::ProcessPath::Contracts::EllipsePrimitive&>(ellipse), param);
}

}  // namespace Siligen::Domain::Trajectory::ValueObjects
