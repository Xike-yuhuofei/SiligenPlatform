#include "TrajectoryShaper.h"

#include "process_path/contracts/GeometryUtils.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ArcPrimitive;
using Siligen::ProcessPath::Contracts::ArcTangent;
using Siligen::ProcessPath::Contracts::Clamp;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::LineDirection;
using Siligen::ProcessPath::Contracts::LinePrimitive;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::NormalizeSweep;
using Siligen::ProcessPath::Contracts::ProcessConstraint;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

namespace {
constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kPi = 3.14159265359f;
constexpr float32 kDegToRad = kPi / 180.0f;
constexpr float32 kRadToDeg = 180.0f / kPi;

Point2D ArcTangentAt(float32 angle_deg, bool clockwise) {
    float32 angle_rad = angle_deg * kDegToRad;
    if (clockwise) {
        return Point2D(std::sin(angle_rad), -std::cos(angle_rad));
    }
    return Point2D(-std::sin(angle_rad), std::cos(angle_rad));
}

bool IsAngleOnArc(const ArcPrimitive& arc, float32 angle_deg) {
    float32 total = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    float32 sweep = ComputeArcSweep(arc.start_angle_deg, angle_deg, arc.clockwise);
    return sweep <= total + 1e-3f;
}

struct LineArcBlendSolution {
    Point2D center{};
    Point2D line_point{};
    Point2D arc_point{};
    float32 line_trim = 0.0f;
    float32 arc_trim = 0.0f;
    float32 radius = 0.0f;
    float32 arc_angle_deg = 0.0f;
    float32 start_angle_deg = 0.0f;
    float32 end_angle_deg = 0.0f;
    bool clockwise = false;
};

int LineCircleIntersections(const Point2D& line_point,
                            const Point2D& line_dir,
                            const Point2D& center,
                            float32 radius,
                            Point2D& out_a,
                            Point2D& out_b) {
    Point2D f = line_point - center;
    float32 b = 2.0f * f.Dot(line_dir);
    float32 c = f.Dot(f) - radius * radius;
    float32 disc = b * b - 4.0f * c;
    if (disc < -kEpsilon) {
        return 0;
    }
    float32 sqrt_disc = std::sqrt(std::max(0.0f, disc));
    float32 t1 = (-b - sqrt_disc) * 0.5f;
    float32 t2 = (-b + sqrt_disc) * 0.5f;
    out_a = line_point + line_dir * t1;
    out_b = line_point + line_dir * t2;
    if (disc <= kEpsilon) {
        return 1;
    }
    return 2;
}

bool EvaluateLineArcCandidate(const Point2D& center,
                              const LinePrimitive& line,
                              const ArcPrimitive& arc,
                              bool line_first,
                              float32 radius,
                              float32 line_length,
                              float32 arc_length,
                              const Point2D& line_dir,
                              LineArcBlendSolution& solution) {
    float32 t = (center - line.start).Dot(line_dir);
    Point2D line_point = line.start + line_dir * t;
    if (t < -1e-3f || t > line_length + 1e-3f) {
        return false;
    }

    float32 line_trim = line_first ? (line_length - t) : t;
    if (line_trim < 1e-3f || line_trim > line_length * 0.5f + 1e-3f) {
        return false;
    }

    Point2D v = center - arc.center;
    float32 dist = v.Length();
    if (dist <= kEpsilon) {
        return false;
    }
    Point2D arc_point = arc.center + v * (arc.radius / dist);
    float32 arc_angle = std::atan2(arc_point.y - arc.center.y, arc_point.x - arc.center.x) * kRadToDeg;
    arc_angle = NormalizeAngle(arc_angle);
    if (!IsAngleOnArc(arc, arc_angle)) {
        return false;
    }

    float32 trim_sweep = line_first ? ComputeArcSweep(arc.start_angle_deg, arc_angle, arc.clockwise)
                                    : ComputeArcSweep(arc_angle, arc.end_angle_deg, arc.clockwise);
    float32 arc_trim = arc.radius * trim_sweep * kDegToRad;
    if (arc_trim < 1e-3f || arc_trim > arc_length * 0.5f + 1e-3f || arc_length - arc_trim < 1e-3f) {
        return false;
    }

    Point2D start_point = line_first ? line_point : arc_point;
    Point2D end_point = line_first ? arc_point : line_point;
    Point2D start_target =
        line_first ? line_dir : ArcTangent(arc, arc_angle).Normalized();
    Point2D end_target =
        line_first ? ArcTangent(arc, arc_angle).Normalized() : line_dir;

    float32 start_angle = std::atan2(start_point.y - center.y, start_point.x - center.x) * kRadToDeg;
    float32 end_angle = std::atan2(end_point.y - center.y, end_point.x - center.x) * kRadToDeg;
    start_angle = NormalizeAngle(start_angle);
    end_angle = NormalizeAngle(end_angle);

    Point2D cw_start = ArcTangentAt(start_angle, true).Normalized();
    Point2D cw_end = ArcTangentAt(end_angle, true).Normalized();
    Point2D ccw_start = ArcTangentAt(start_angle, false).Normalized();
    Point2D ccw_end = ArcTangentAt(end_angle, false).Normalized();
    float32 score_cw = cw_start.Dot(start_target) + cw_end.Dot(end_target);
    float32 score_ccw = ccw_start.Dot(start_target) + ccw_end.Dot(end_target);
    bool clockwise = score_cw >= score_ccw;
    float32 dot_start = clockwise ? cw_start.Dot(start_target) : ccw_start.Dot(start_target);
    float32 dot_end = clockwise ? cw_end.Dot(end_target) : ccw_end.Dot(end_target);
    if (dot_start < 0.0f || dot_end < 0.0f) {
        return false;
    }

    float32 sweep = ComputeArcSweep(start_angle, end_angle, clockwise);
    if (sweep <= 1e-3f || sweep > 180.0f + 1e-3f) {
        return false;
    }

    solution.center = center;
    solution.line_point = line_point;
    solution.arc_point = arc_point;
    solution.line_trim = line_trim;
    solution.arc_trim = arc_trim;
    solution.radius = radius;
    solution.arc_angle_deg = arc_angle;
    solution.start_angle_deg = start_angle;
    solution.end_angle_deg = end_angle;
    solution.clockwise = clockwise;
    return true;
}

bool FindLineArcBlendSolution(const LinePrimitive& line,
                              const ArcPrimitive& arc,
                              bool line_first,
                              const TrajectoryShaperConfig& config,
                              LineArcBlendSolution& best_solution) {
    float32 line_length = line.start.DistanceTo(line.end);
    if (line_length <= kEpsilon || arc.radius <= kEpsilon) {
        return false;
    }

    float32 arc_length = ComputeArcLength(arc);
    if (arc_length <= kEpsilon) {
        return false;
    }

    Point2D line_dir = LineDirection(line);
    if (line_dir.Length() <= kEpsilon) {
        return false;
    }

    Point2D arc_joint = line_first ? ArcPoint(arc, arc.start_angle_deg)
                                   : ArcPoint(arc, arc.end_angle_deg);
    Point2D line_joint = line_first ? line.end : line.start;
    if (line_joint.DistanceTo(arc_joint) > config.position_tolerance) {
        return false;
    }

    Point2D arc_tangent = line_first ? ArcTangent(arc, arc.start_angle_deg).Normalized()
                                     : ArcTangent(arc, arc.end_angle_deg).Normalized();
    float32 dot = Clamp(line_dir.Dot(arc_tangent), -1.0f, 1.0f);
    float32 angle = std::acos(dot);
    if (angle < 1e-3f || std::abs(kPi - angle) < 1e-3f) {
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

    Point2D normal(-line_dir.y, line_dir.x);
    Point2D base = line.start;
    Point2D offset_points[2] = {base + normal * radius, base - normal * radius};
    float32 dist_candidates[2] = {arc.radius + radius, std::abs(arc.radius - radius)};

    bool found = false;
    float32 best_trim = 0.0f;

    for (const auto& offset_point : offset_points) {
        for (float32 dist : dist_candidates) {
            if (dist <= kEpsilon) {
                continue;
            }
            Point2D i0;
            Point2D i1;
            int count = LineCircleIntersections(offset_point, line_dir, arc.center, dist, i0, i1);
            if (count <= 0) {
                continue;
            }
            for (int idx = 0; idx < count; ++idx) {
                const Point2D& center = (idx == 0) ? i0 : i1;
                LineArcBlendSolution candidate;
                if (!EvaluateLineArcCandidate(center,
                                              line,
                                              arc,
                                              line_first,
                                              radius,
                                              line_length,
                                              arc_length,
                                              line_dir,
                                              candidate)) {
                    continue;
                }
                float32 total_trim = candidate.line_trim + candidate.arc_trim;
                if (!found || total_trim < best_trim) {
                    best_trim = total_trim;
                    best_solution = candidate;
                    found = true;
                }
            }
        }
    }

    return found;
}

void ResetCornerTag(ProcessSegment& segment) {
    if (segment.tag != ProcessTag::Corner) {
        return;
    }
    segment.tag = ProcessTag::Normal;
    segment.constraint.has_velocity_factor = false;
    segment.constraint.max_velocity_factor = 1.0f;
    segment.constraint.has_local_speed_hint = false;
    segment.constraint.local_speed_factor = 1.0f;
}

void ApplyBlendConstraint(ProcessSegment& blend_arc,
                          const ProcessSegment& prev,
                          const ProcessSegment& next) {
    if (prev.constraint.has_velocity_factor || next.constraint.has_velocity_factor) {
        float32 factor = prev.constraint.has_velocity_factor ? prev.constraint.max_velocity_factor
                                                              : next.constraint.max_velocity_factor;
        if (prev.constraint.has_velocity_factor && next.constraint.has_velocity_factor) {
            factor = std::min(prev.constraint.max_velocity_factor, next.constraint.max_velocity_factor);
        }
        blend_arc.constraint.has_velocity_factor = true;
        blend_arc.constraint.max_velocity_factor = factor;
    } else {
        blend_arc.constraint.has_velocity_factor = false;
        blend_arc.constraint.max_velocity_factor = 1.0f;
    }
    blend_arc.constraint.has_local_speed_hint = true;
    blend_arc.constraint.local_speed_factor =
        blend_arc.constraint.has_velocity_factor ? blend_arc.constraint.max_velocity_factor : 1.0f;
}

bool CanBlendLineLine(const ProcessSegment& prev,
                      const ProcessSegment& next,
                      const TrajectoryShaperConfig& config,
                      ProcessSegment& blended_prev,
                      ProcessSegment& blend_arc,
                      ProcessSegment& blended_next) {
    if (prev.geometry.type != SegmentType::Line || next.geometry.type != SegmentType::Line) {
        return false;
    }
    if (prev.geometry.is_spline_approx || next.geometry.is_spline_approx) {
        return false;
    }
    if (!prev.dispense_on || !next.dispense_on) {
        return false;
    }

    const Point2D junction = prev.geometry.line.end;
    if (junction.DistanceTo(next.geometry.line.start) > config.position_tolerance) {
        return false;
    }

    Point2D v1 = prev.geometry.line.end - prev.geometry.line.start;
    Point2D v2 = next.geometry.line.end - next.geometry.line.start;
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
    const float32 min_angle = kMinBlendAngleDeg * kDegToRad;
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

    // The blend center lies on the bisector between the reversed incoming
    // direction and the outgoing direction. Using dir1 + dir2 pushes the
    // center outside the corner and can produce a major arc sweep.
    Point2D bis = (dir2 - dir1);
    float32 bis_len = bis.Length();
    if (bis_len < kEpsilon) {
        return false;
    }
    bis = bis / bis_len;
    float32 center_dist = radius / std::sin(angle * 0.5f);
    Point2D center = junction + bis * center_dist;

    bool clockwise = (dir1.Cross(dir2) < 0.0f);
    float32 start_angle = std::atan2(prev_end.y - center.y, prev_end.x - center.x) * kRadToDeg;
    float32 end_angle = std::atan2(next_start.y - center.y, next_start.x - center.x) * kRadToDeg;
    start_angle = NormalizeAngle(start_angle);
    end_angle = NormalizeAngle(end_angle);

    blended_prev = prev;
    blended_prev.geometry.line.end = prev_end;
    blended_prev.geometry.length = blended_prev.geometry.line.start.DistanceTo(prev_end);
    ResetCornerTag(blended_prev);

    blended_next = next;
    blended_next.geometry.line.start = next_start;
    blended_next.geometry.length = blended_next.geometry.line.start.DistanceTo(blended_next.geometry.line.end);
    ResetCornerTag(blended_next);

    blend_arc = prev;
    blend_arc.geometry.type = SegmentType::Arc;
    blend_arc.geometry.arc.center = center;
    blend_arc.geometry.arc.radius = radius;
    blend_arc.geometry.arc.start_angle_deg = start_angle;
    blend_arc.geometry.arc.end_angle_deg = end_angle;
    blend_arc.geometry.arc.clockwise = clockwise;
    blend_arc.geometry.length = ComputeArcLength(blend_arc.geometry.arc);
    blend_arc.tag = ProcessTag::Corner;
    blend_arc.dispense_on = prev.dispense_on && next.dispense_on;
    blend_arc.flow_rate = std::min(prev.flow_rate, next.flow_rate);
    ApplyBlendConstraint(blend_arc, prev, next);

    return true;
}

bool CanBlendLineArc(const ProcessSegment& prev,
                     const ProcessSegment& next,
                     const TrajectoryShaperConfig& config,
                     ProcessSegment& blended_prev,
                     ProcessSegment& blend_arc,
                     ProcessSegment& blended_next) {
    if (prev.geometry.type != SegmentType::Line || next.geometry.type != SegmentType::Arc) {
        return false;
    }
    if (prev.geometry.is_spline_approx || next.geometry.is_spline_approx) {
        return false;
    }
    if (!prev.dispense_on || !next.dispense_on) {
        return false;
    }

    LineArcBlendSolution solution;
    if (!FindLineArcBlendSolution(prev.geometry.line, next.geometry.arc, true, config, solution)) {
        return false;
    }

    blended_prev = prev;
    blended_prev.geometry.line.end = solution.line_point;
    blended_prev.geometry.length = blended_prev.geometry.line.start.DistanceTo(solution.line_point);
    ResetCornerTag(blended_prev);

    blended_next = next;
    blended_next.geometry.arc.start_angle_deg = solution.arc_angle_deg;
    blended_next.geometry.length = ComputeArcLength(blended_next.geometry.arc);
    ResetCornerTag(blended_next);

    blend_arc = prev;
    blend_arc.geometry.type = SegmentType::Arc;
    blend_arc.geometry.arc.center = solution.center;
    blend_arc.geometry.arc.radius = solution.radius;
    blend_arc.geometry.arc.start_angle_deg = solution.start_angle_deg;
    blend_arc.geometry.arc.end_angle_deg = solution.end_angle_deg;
    blend_arc.geometry.arc.clockwise = solution.clockwise;
    blend_arc.geometry.length = ComputeArcLength(blend_arc.geometry.arc);
    blend_arc.geometry.is_spline_approx = false;
    blend_arc.tag = ProcessTag::Corner;
    blend_arc.dispense_on = prev.dispense_on && next.dispense_on;
    blend_arc.flow_rate = std::min(prev.flow_rate, next.flow_rate);
    ApplyBlendConstraint(blend_arc, prev, next);

    return true;
}

bool CanBlendArcLine(const ProcessSegment& prev,
                     const ProcessSegment& next,
                     const TrajectoryShaperConfig& config,
                     ProcessSegment& blended_prev,
                     ProcessSegment& blend_arc,
                     ProcessSegment& blended_next) {
    if (prev.geometry.type != SegmentType::Arc || next.geometry.type != SegmentType::Line) {
        return false;
    }
    if (prev.geometry.is_spline_approx || next.geometry.is_spline_approx) {
        return false;
    }
    if (!prev.dispense_on || !next.dispense_on) {
        return false;
    }

    LineArcBlendSolution solution;
    if (!FindLineArcBlendSolution(next.geometry.line, prev.geometry.arc, false, config, solution)) {
        return false;
    }

    blended_prev = prev;
    blended_prev.geometry.arc.end_angle_deg = solution.arc_angle_deg;
    blended_prev.geometry.length = ComputeArcLength(blended_prev.geometry.arc);
    ResetCornerTag(blended_prev);

    blended_next = next;
    blended_next.geometry.line.start = solution.line_point;
    blended_next.geometry.length = blended_next.geometry.line.start.DistanceTo(blended_next.geometry.line.end);
    ResetCornerTag(blended_next);

    blend_arc = prev;
    blend_arc.geometry.type = SegmentType::Arc;
    blend_arc.geometry.arc.center = solution.center;
    blend_arc.geometry.arc.radius = solution.radius;
    blend_arc.geometry.arc.start_angle_deg = solution.start_angle_deg;
    blend_arc.geometry.arc.end_angle_deg = solution.end_angle_deg;
    blend_arc.geometry.arc.clockwise = solution.clockwise;
    blend_arc.geometry.length = ComputeArcLength(blend_arc.geometry.arc);
    blend_arc.geometry.is_spline_approx = false;
    blend_arc.tag = ProcessTag::Corner;
    blend_arc.dispense_on = prev.dispense_on && next.dispense_on;
    blend_arc.flow_rate = std::min(prev.flow_rate, next.flow_rate);
    ApplyBlendConstraint(blend_arc, prev, next);

    return true;
}

ProcessConstraint MergeConstraints(const ProcessConstraint& a, const ProcessConstraint& b) {
    ProcessConstraint merged{};
    if (a.has_velocity_factor || b.has_velocity_factor) {
        merged.has_velocity_factor = true;
        if (a.has_velocity_factor && b.has_velocity_factor) {
            merged.max_velocity_factor = std::min(a.max_velocity_factor, b.max_velocity_factor);
        } else {
            merged.max_velocity_factor = a.has_velocity_factor ? a.max_velocity_factor : b.max_velocity_factor;
        }
    }
    if (a.has_local_speed_hint || b.has_local_speed_hint) {
        merged.has_local_speed_hint = true;
        if (a.has_local_speed_hint && b.has_local_speed_hint) {
            merged.local_speed_factor = std::min(a.local_speed_factor, b.local_speed_factor);
        } else {
            merged.local_speed_factor = a.has_local_speed_hint ? a.local_speed_factor : b.local_speed_factor;
        }
    }
    return merged;
}

bool AreCollinearLines(const LinePrimitive& a, const LinePrimitive& b, float32 tolerance) {
    Point2D dir1 = a.end - a.start;
    Point2D dir2 = b.end - b.start;
    float32 len1 = dir1.Length();
    float32 len2 = dir2.Length();
    if (len1 <= kEpsilon || len2 <= kEpsilon) {
        return false;
    }
    float32 cross_dir = std::abs(dir1.Cross(dir2));
    if (cross_dir > tolerance * len1 * len2) {
        return false;
    }
    float32 cross_offset = std::abs((b.start - a.start).Cross(dir1));
    if (cross_offset > tolerance * len1) {
        return false;
    }
    return true;
}

ProcessPath SimplifyRapidSegments(const ProcessPath& input, float32 tolerance) {
    ProcessPath output;
    output.segments.reserve(input.segments.size());
    for (const auto& seg : input.segments) {
        if (output.segments.empty()) {
            output.segments.push_back(seg);
            continue;
        }

        auto& prev = output.segments.back();
        if (!prev.dispense_on && !seg.dispense_on &&
            prev.geometry.type == SegmentType::Line &&
            seg.geometry.type == SegmentType::Line &&
            AreCollinearLines(prev.geometry.line, seg.geometry.line, tolerance) &&
            SegmentEnd(prev.geometry).DistanceTo(SegmentStart(seg.geometry)) <= tolerance) {
            prev.geometry.line.end = seg.geometry.line.end;
            prev.geometry.length = prev.geometry.line.start.DistanceTo(prev.geometry.line.end);
            prev.constraint = MergeConstraints(prev.constraint, seg.constraint);
            prev.tag = ProcessTag::Rapid;
            prev.dispense_on = false;
            prev.flow_rate = 0.0f;
            continue;
        }

        output.segments.push_back(seg);
    }
    return output;
}

}  // namespace

ProcessPath TrajectoryShaper::Shape(const ProcessPath& input, const TrajectoryShaperConfig& config) const {
    ProcessPath output;
    if (input.segments.size() < 2) {
        return input;
    }

    ProcessSegment prev = input.segments.front();
    for (size_t i = 1; i < input.segments.size(); ++i) {
        const auto& next = input.segments[i];
        ProcessSegment blended_prev;
        ProcessSegment blend_arc;
        ProcessSegment blended_next;

        if (CanBlendLineLine(prev, next, config, blended_prev, blend_arc, blended_next) ||
            CanBlendLineArc(prev, next, config, blended_prev, blend_arc, blended_next) ||
            CanBlendArcLine(prev, next, config, blended_prev, blend_arc, blended_next)) {
            output.segments.push_back(blended_prev);
            output.segments.push_back(blend_arc);
            prev = blended_next;
        } else {
            output.segments.push_back(prev);
            prev = next;
        }
    }

    output.segments.push_back(prev);
    return SimplifyRapidSegments(output, config.position_tolerance);
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
