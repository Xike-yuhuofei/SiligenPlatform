#include "ProcessAnnotator.h"

#include "domain/trajectory/value-objects/GeometryUtils.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive;
using Siligen::Domain::Trajectory::ValueObjects::LinePrimitive;
using Siligen::Domain::Trajectory::ValueObjects::ArcTangent;
using Siligen::Domain::Trajectory::ValueObjects::Clamp;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::LineDirection;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle;
using Siligen::Domain::Trajectory::ValueObjects::SegmentEnd;
using Siligen::Domain::Trajectory::ValueObjects::SegmentStart;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::kRadToDeg;

namespace {
constexpr float32 kEpsilon = 1e-6f;

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }
    if (segment.type == SegmentType::Line) {
        return segment.line.start.DistanceTo(segment.line.end);
    }
    return ComputeArcLength(segment.arc);
}

bool IsCurveChainCandidate(const Segment& prev_seg,
                           const Segment& curr_seg,
                           float32 angle_deg,
                           const ProcessConfig& config) {
    if (config.curve_chain_angle_deg <= kEpsilon || config.curve_chain_max_segment_mm <= kEpsilon) {
        return false;
    }
    if (prev_seg.type != SegmentType::Line || curr_seg.type != SegmentType::Line) {
        return false;
    }
    if (angle_deg > config.curve_chain_angle_deg) {
        return false;
    }
    float32 prev_len = SegmentLength(prev_seg);
    float32 curr_len = SegmentLength(curr_seg);
    float32 min_len = std::min(prev_len, curr_len);
    return min_len <= config.curve_chain_max_segment_mm;
}

Point2D SegmentTangentAtStart(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return LineDirection(segment.line);
    }
    return ArcTangent(segment.arc, segment.arc.start_angle_deg).Normalized();
}

Point2D SegmentTangentAtEnd(const Segment& segment) {
    if (segment.type == SegmentType::Line) {
        return LineDirection(segment.line);
    }
    return ArcTangent(segment.arc, segment.arc.end_angle_deg).Normalized();
}

float32 ResolveSpeedFactor(ProcessTag tag, const ProcessConfig& config) {
    switch (tag) {
        case ProcessTag::Start:
            return config.start_speed_factor;
        case ProcessTag::End:
            return config.end_speed_factor;
        case ProcessTag::Corner:
            return config.corner_slowdown ? config.corner_speed_factor : 1.0f;
        case ProcessTag::Rapid:
            return config.rapid_speed_factor;
        case ProcessTag::Normal:
        default:
            return 1.0f;
    }
}

void ApplyVelocityConstraint(ProcessSegment& segment, ProcessTag tag, const ProcessConfig& config) {
    if (tag == ProcessTag::Normal) {
        return;
    }
    segment.constraint.has_velocity_factor = true;
    segment.constraint.max_velocity_factor = ResolveSpeedFactor(tag, config);
}

Segment MakeLineSegment(const Point2D& start, const Point2D& end) {
    Segment seg;
    seg.type = SegmentType::Line;
    seg.line.start = start;
    seg.line.end = end;
    seg.length = start.DistanceTo(end);
    seg.curvature_radius = 0.0f;
    seg.is_spline_approx = false;
    return seg;
}

Segment SplitArcSegment(const Segment& arc_seg, float32 split_dist, bool first_part) {
    Segment seg = arc_seg;
    float32 length = SegmentLength(arc_seg);
    if (length <= kEpsilon || arc_seg.type != SegmentType::Arc) {
        return seg;
    }
    float32 ratio = Clamp(split_dist / length, 0.0f, 1.0f);
    float32 sweep = ComputeArcSweep(arc_seg.arc.start_angle_deg,
                                    arc_seg.arc.end_angle_deg,
                                    arc_seg.arc.clockwise);
    float32 offset = sweep * ratio;
    float32 split_angle = arc_seg.arc.clockwise
                              ? arc_seg.arc.start_angle_deg - offset
                              : arc_seg.arc.start_angle_deg + offset;
    split_angle = NormalizeAngle(split_angle);

    if (first_part) {
        seg.arc.end_angle_deg = split_angle;
    } else {
        seg.arc.start_angle_deg = split_angle;
    }
    seg.length = ComputeArcLength(seg.arc);
    return seg;
}

}  // namespace

ProcessPath ProcessAnnotator::Annotate(const Path& path, const ProcessConfig& config) const {
    ProcessPath output;
    if (path.segments.empty()) {
        return output;
    }

    std::vector<Segment> segments = path.segments;

    // lead-on segment
    if (config.lead_on_dist > kEpsilon) {
        const Segment& first = segments.front();
        Point2D start = SegmentStart(first);
        Point2D dir = SegmentTangentAtStart(first);
        if (dir.Length() > kEpsilon) {
            Point2D lead_start = start - dir * config.lead_on_dist;
            ProcessSegment lead_seg;
            lead_seg.geometry = MakeLineSegment(lead_start, start);
            lead_seg.dispense_on = true;
            lead_seg.flow_rate = config.default_flow;
            lead_seg.tag = ProcessTag::Start;
            ApplyVelocityConstraint(lead_seg, lead_seg.tag, config);
            output.segments.push_back(lead_seg);
        }
    }

    for (size_t i = 0; i < segments.size(); ++i) {
        Segment seg = segments[i];
        float32 length = SegmentLength(seg);
        if (length <= kEpsilon) {
            continue;
        }

        if (!output.segments.empty()) {
            Point2D prev_end = SegmentEnd(output.segments.back().geometry);
            Point2D curr_start = SegmentStart(seg);
            float32 gap = prev_end.DistanceTo(curr_start);
            if (gap > config.rapid_gap_threshold) {
                ProcessSegment rapid_seg;
                rapid_seg.geometry = MakeLineSegment(prev_end, curr_start);
                rapid_seg.dispense_on = false;
                rapid_seg.flow_rate = 0.0f;
                rapid_seg.tag = ProcessTag::Rapid;
                ApplyVelocityConstraint(rapid_seg, rapid_seg.tag, config);
                output.segments.push_back(rapid_seg);
            }
        }

        ProcessTag tag = ProcessTag::Normal;
        if (output.segments.empty()) {
            tag = ProcessTag::Start;
        }
        if (i == segments.size() - 1) {
            tag = ProcessTag::End;
        }

        if (i > 0 && config.corner_slowdown) {
            const Segment& prev_seg = segments[i - 1];
            Point2D prev_dir = SegmentTangentAtEnd(prev_seg);
            Point2D curr_dir = SegmentTangentAtStart(seg);
            float32 dot = Clamp(prev_dir.Dot(curr_dir), -1.0f, 1.0f);
            float32 angle_rad = std::acos(dot);
            float32 angle_deg = angle_rad * kRadToDeg;
            const bool is_spline_chain = prev_seg.is_spline_approx && seg.is_spline_approx;
            const bool is_curve_chain = IsCurveChainCandidate(prev_seg, seg, angle_deg, config);
            if (!is_spline_chain && !is_curve_chain &&
                angle_deg > config.corner_angle_deg && angle_deg < (180.0f - config.corner_angle_deg)) {
                if (tag == ProcessTag::Normal) {
                    tag = ProcessTag::Corner;
                }
            }
        }

        if (i == segments.size() - 1 && config.lead_off_dist > kEpsilon && length > config.lead_off_dist) {
            float32 split_dist = length - config.lead_off_dist;
            ProcessSegment main_seg;
            ProcessSegment tail_seg;

            if (seg.type == SegmentType::Line) {
                Point2D dir = LineDirection(seg.line);
                Point2D split_point = seg.line.start + dir * split_dist;
                main_seg.geometry = MakeLineSegment(seg.line.start, split_point);
                tail_seg.geometry = MakeLineSegment(split_point, seg.line.end);
                if (seg.is_spline_approx) {
                    main_seg.geometry.is_spline_approx = true;
                    tail_seg.geometry.is_spline_approx = true;
                    main_seg.geometry.curvature_radius = seg.curvature_radius;
                    tail_seg.geometry.curvature_radius = seg.curvature_radius;
                }
            } else {
                main_seg.geometry = SplitArcSegment(seg, split_dist, true);
                tail_seg.geometry = SplitArcSegment(seg, split_dist, false);
            }

            main_seg.dispense_on = true;
            main_seg.flow_rate = config.default_flow;
            main_seg.tag = (tag == ProcessTag::End) ? ProcessTag::Normal : tag;
            ApplyVelocityConstraint(main_seg, main_seg.tag, config);

            tail_seg.dispense_on = false;
            tail_seg.flow_rate = 0.0f;
            tail_seg.tag = ProcessTag::End;
            ApplyVelocityConstraint(tail_seg, tail_seg.tag, config);

            output.segments.push_back(main_seg);
            output.segments.push_back(tail_seg);
        } else {
            ProcessSegment proc_seg;
            proc_seg.geometry = seg;
            proc_seg.dispense_on = true;
            proc_seg.flow_rate = config.default_flow;
            proc_seg.tag = tag;
            ApplyVelocityConstraint(proc_seg, proc_seg.tag, config);
            output.segments.push_back(proc_seg);
        }
    }

    return output;
}

}  // namespace Siligen::Domain::Trajectory::DomainServices
