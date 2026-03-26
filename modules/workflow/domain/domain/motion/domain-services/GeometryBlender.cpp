#include "GeometryBlender.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion {

namespace {
constexpr float32 kEpsilon = 1e-5f;
constexpr float32 kPi = 3.14159265359f;

float32 Clamp(float32 value, float32 lo, float32 hi) {
    return std::min(std::max(value, lo), hi);
}

bool IsLinear(const MotionSegment& segment) {
    return segment.segment_type == SegmentType::LINEAR;
}
}  // namespace

std::vector<MotionSegment> GeometryBlender::BlendSegments(const std::vector<MotionSegment>& segments,
                                                          const InterpolationConfig& config) const {
    if (!config.enable_smoothing || segments.size() < 2) {
        return segments;
    }

    std::vector<MotionSegment> blended;
    blended.reserve(segments.size() + 8);

    MotionSegment prev = segments.front();
    for (size_t i = 1; i < segments.size(); ++i) {
        const MotionSegment& next = segments[i];
        MotionSegment blended_prev;
        MotionSegment blend_arc;
        MotionSegment blended_next;

        if (IsLinear(prev) && IsLinear(next) &&
            TryBlendCorner(prev, next, config, blended_prev, blend_arc, blended_next)) {
            blended.push_back(blended_prev);
            blended.push_back(blend_arc);
            prev = blended_next;
        } else {
            blended.push_back(prev);
            prev = next;
        }
    }

    blended.push_back(prev);
    return blended;
}

bool GeometryBlender::TryBlendCorner(const MotionSegment& prev,
                                     const MotionSegment& next,
                                     const InterpolationConfig& config,
                                     MotionSegment& blended_prev,
                                     MotionSegment& blend_arc,
                                     MotionSegment& blended_next) const {
    const Point2D junction = prev.end_point;
    if (junction.DistanceTo(next.start_point) > config.position_tolerance) {
        return false;
    }

    Point2D v1 = prev.end_point - prev.start_point;
    Point2D v2 = next.end_point - next.start_point;
    float32 len1 = v1.Length();
    float32 len2 = v2.Length();
    if (len1 < kEpsilon || len2 < kEpsilon) {
        return false;
    }

    Point2D dir1 = v1 / len1;
    Point2D dir2 = v2 / len2;
    float32 dot = Clamp(dir1.Dot(dir2), -1.0f, 1.0f);
    float32 angle = std::acos(dot);
    constexpr float32 kMinBlendAngleDeg = 5.0f;
    const float32 min_angle = kMinBlendAngleDeg * (kPi / 180.0f);
    if (angle < min_angle || std::abs(kPi - angle) < 1e-3f) {
        return false;
    }

    float32 radius = config.corner_smoothing_radius;
    float32 deviation_denom = 1.0f - std::cos(angle * 0.5f);
    if (config.corner_max_deviation_mm > kEpsilon && deviation_denom > kEpsilon) {
        radius = std::min(radius, config.corner_max_deviation_mm / deviation_denom);
    }

    if (radius < config.corner_min_radius_mm) {
        return false;
    }

    float32 half_angle = angle * 0.5f;
    float32 tan_half = std::tan(half_angle);
    if (tan_half <= kEpsilon) {
        return false;
    }

    float32 trim = radius / tan_half;
    float32 max_trim = std::min(len1, len2) * 0.5f;
    if (trim > max_trim) {
        trim = max_trim;
        radius = trim * tan_half;
        if (radius < config.corner_min_radius_mm) {
            return false;
        }
    }

    Point2D prev_end = junction - dir1 * trim;
    Point2D next_start = junction + dir2 * trim;

    blended_prev = prev;
    blended_prev.end_point = prev_end;
    blended_prev.length = blended_prev.start_point.DistanceTo(blended_prev.end_point);

    blended_next = next;
    blended_next.start_point = next_start;
    blended_next.length = blended_next.start_point.DistanceTo(blended_next.end_point);

    blend_arc = MotionSegment(prev_end, next_start);
    blend_arc.length = radius * angle;
    blend_arc.curvature_radius = radius;
    blend_arc.segment_type = (dir1.Cross(dir2) < 0.0f) ? SegmentType::ARC_CW : SegmentType::ARC_CCW;
    blend_arc.max_velocity = std::min(prev.max_velocity, next.max_velocity);
    blend_arc.max_acceleration = std::min(prev.max_acceleration, next.max_acceleration);
    blend_arc.enable_dispensing = prev.enable_dispensing && next.enable_dispensing;

    return true;
}

}  // namespace Siligen::Domain::Motion
