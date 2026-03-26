#include "UnifiedTrajectoryPlannerService.h"

#include "domain/trajectory/domain-services/MotionPlanner.h"

#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices {

namespace {
constexpr float32 kPi = 3.14159265359f;
constexpr float32 kDegToRad = kPi / 180.0f;
constexpr float32 kEpsilon = 1e-6f;

using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::Point2D;

Point2D ArcPoint(const Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive& arc,
                 float32 angle_deg) {
    float32 angle_rad = angle_deg * kDegToRad;
    return Point2D(arc.center.x + arc.radius * std::cos(angle_rad),
                   arc.center.y + arc.radius * std::sin(angle_rad));
}

Point2D SegmentStart(const Segment& segment) {
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.start;
        case SegmentType::Arc:
            return ArcPoint(segment.arc, segment.arc.start_angle_deg);
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.front();
            }
            return Point2D();
        default:
            return Point2D();
    }
}

Point2D SegmentEnd(const Segment& segment) {
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.end;
        case SegmentType::Arc:
            return ArcPoint(segment.arc, segment.arc.end_angle_deg);
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.back();
            }
            return Point2D();
        default:
            return Point2D();
    }
}

void OptimizeLineOrientation(Siligen::Domain::Trajectory::ValueObjects::Path& path,
                             float32 tolerance) {
    if (path.segments.size() < 2) {
        return;
    }

    for (size_t i = 0; i < path.segments.size(); ++i) {
        auto& seg = path.segments[i];
        if (seg.type != SegmentType::Line) {
            continue;
        }

        const Point2D prev_end = (i > 0) ? SegmentEnd(path.segments[i - 1]) : seg.line.start;
        const Point2D next_start =
            (i + 1 < path.segments.size()) ? SegmentStart(path.segments[i + 1]) : seg.line.end;

        const Point2D start = seg.line.start;
        const Point2D end = seg.line.end;

        const float32 cost_current = prev_end.DistanceTo(start) + end.DistanceTo(next_start);
        const float32 cost_reversed = prev_end.DistanceTo(end) + start.DistanceTo(next_start);

        if (cost_reversed + std::max(tolerance, kEpsilon) < cost_current) {
            std::swap(seg.line.start, seg.line.end);
        }
    }
}
}  // namespace

UnifiedTrajectoryPlannerService::UnifiedTrajectoryPlannerService(
    std::shared_ptr<Domain::Motion::DomainServices::VelocityProfileService> velocity_service)
    : velocity_service_(std::move(velocity_service)) {}

UnifiedTrajectoryPlanResult UnifiedTrajectoryPlannerService::Plan(
    const std::vector<Domain::Trajectory::ValueObjects::Primitive>& primitives,
    const UnifiedTrajectoryPlanRequest& request) const {
    UnifiedTrajectoryPlanResult result;

    Domain::Trajectory::DomainServices::GeometryNormalizer normalizer;
    result.normalized = normalizer.Normalize(primitives, request.normalization);
    OptimizeLineOrientation(result.normalized.path, request.normalization.continuity_tolerance);

    Domain::Trajectory::DomainServices::ProcessAnnotator annotator;
    result.process_path = annotator.Annotate(result.normalized.path, request.process);

    Domain::Trajectory::DomainServices::TrajectoryShaper shaper;
    result.shaped_path = shaper.Shape(result.process_path, request.shaping);

    if (request.generate_motion_trajectory) {
        Domain::Trajectory::DomainServices::MotionPlanner planner(velocity_service_);
        result.motion_trajectory = planner.Plan(result.shaped_path, request.motion);
    }

    return result;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices


