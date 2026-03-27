#include "TimeTrajectoryPlanner.h"

#include "domain/motion/domain-services/MotionPlanner.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Domain::Motion::DomainServices::MotionPlanner;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
using Siligen::Domain::Motion::ValueObjects::SemanticSegment;
using Siligen::Domain::Motion::ValueObjects::SemanticTag;

namespace {
ProcessTag MapProcessTag(SemanticTag tag) {
    switch (tag) {
        case SemanticTag::Start:
            return ProcessTag::Start;
        case SemanticTag::End:
            return ProcessTag::End;
        case SemanticTag::Corner:
            return ProcessTag::Corner;
        case SemanticTag::Rapid:
            return ProcessTag::Rapid;
        case SemanticTag::Normal:
        default:
            return ProcessTag::Normal;
    }
}

ProcessPath ConvertToProcessPath(const SemanticPath& path) {
    ProcessPath out;
    out.segments.reserve(path.segments.size());

    for (const auto& seg : path.segments) {
        ProcessSegment out_seg;
        out_seg.dispense_on = seg.dispense_on;
        out_seg.flow_rate = seg.flow_rate;
        out_seg.tag = MapProcessTag(seg.tag);

        if (seg.geometry.type == ValueObjects::SegmentType::Line) {
            out_seg.geometry.type = SegmentType::Line;
            out_seg.geometry.line.start = seg.geometry.line.start;
            out_seg.geometry.line.end = seg.geometry.line.end;
        } else {
            out_seg.geometry.type = SegmentType::Arc;
            out_seg.geometry.arc.center = seg.geometry.arc.center;
            out_seg.geometry.arc.radius = seg.geometry.arc.radius;
            out_seg.geometry.arc.start_angle_deg = seg.geometry.arc.start_angle_deg;
            out_seg.geometry.arc.end_angle_deg = seg.geometry.arc.end_angle_deg;
            out_seg.geometry.arc.clockwise = seg.geometry.arc.clockwise;
        }
        out_seg.geometry.length = seg.geometry.length;
        out.segments.push_back(out_seg);
    }

    return out;
}

MotionConfig ConvertConfig(const TimePlanningConfig& config) {
    MotionConfig out;
    out.vmax = config.vmax;
    out.amax = config.amax;
    out.jmax = config.jmax;
    out.sample_ds = config.sample_ds;
    out.sample_dt = config.sample_dt;
    out.curvature_speed_factor = config.curvature_speed_factor;
    out.corner_speed_factor = config.corner_speed_factor;
    out.start_speed_factor = config.start_speed_factor;
    out.end_speed_factor = config.end_speed_factor;
    out.rapid_speed_factor = config.rapid_speed_factor;
    out.enforce_jerk_limit = config.enforce_jerk_limit;
    return out;
}

}  // namespace

MotionTrajectory TimeTrajectoryPlanner::Plan(const SemanticPath& path, const TimePlanningConfig& config) const {
    MotionPlanner planner;
    return planner.Plan(ConvertToProcessPath(path), ConvertConfig(config));
}

MotionTrajectory TimeTrajectoryPlanner::Plan(const ProcessPath& path, const MotionConfig& config) const {
    MotionPlanner planner;
    return planner.Plan(path, config);
}

}  // namespace Siligen::Domain::Motion::DomainServices
