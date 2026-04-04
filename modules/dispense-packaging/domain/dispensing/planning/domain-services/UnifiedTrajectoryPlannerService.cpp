#include "UnifiedTrajectoryPlannerService.h"

#include "domain/motion/domain-services/MotionPlanner.h"
#include "process_path/contracts/GeometryUtils.h"
#include "domain-services/GeometryNormalizer.h"
#include "domain-services/ProcessAnnotator.h"
#include "domain-services/TrajectoryShaper.h"

#include <cmath>

namespace Siligen::Domain::Dispensing::DomainServices {

namespace {
constexpr float32 kEpsilon = 1e-6f;

using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;
namespace Contracts = Siligen::ProcessPath::Contracts;
namespace Owner = Siligen::Domain::Trajectory;

Owner::ValueObjects::LinePrimitive ToOwnerLine(const Contracts::LinePrimitive& line) {
    return Owner::ValueObjects::LinePrimitive{line.start, line.end};
}

Owner::ValueObjects::ArcPrimitive ToOwnerArc(const Contracts::ArcPrimitive& arc) {
    Owner::ValueObjects::ArcPrimitive owner_arc;
    owner_arc.center = arc.center;
    owner_arc.radius = arc.radius;
    owner_arc.start_angle_deg = arc.start_angle_deg;
    owner_arc.end_angle_deg = arc.end_angle_deg;
    owner_arc.clockwise = arc.clockwise;
    return owner_arc;
}

Owner::ValueObjects::SplinePrimitive ToOwnerSpline(const Contracts::SplinePrimitive& spline) {
    Owner::ValueObjects::SplinePrimitive owner_spline;
    owner_spline.control_points = spline.control_points;
    owner_spline.closed = spline.closed;
    return owner_spline;
}

Owner::ValueObjects::Primitive ToOwnerPrimitive(const Contracts::Primitive& primitive) {
    Owner::ValueObjects::Primitive owner_primitive;
    owner_primitive.type = static_cast<Owner::ValueObjects::PrimitiveType>(primitive.type);
    owner_primitive.line = ToOwnerLine(primitive.line);
    owner_primitive.arc = ToOwnerArc(primitive.arc);
    owner_primitive.spline = ToOwnerSpline(primitive.spline);
    owner_primitive.circle.center = primitive.circle.center;
    owner_primitive.circle.radius = primitive.circle.radius;
    owner_primitive.circle.start_angle_deg = primitive.circle.start_angle_deg;
    owner_primitive.circle.clockwise = primitive.circle.clockwise;
    owner_primitive.ellipse.center = primitive.ellipse.center;
    owner_primitive.ellipse.major_axis = primitive.ellipse.major_axis;
    owner_primitive.ellipse.ratio = primitive.ellipse.ratio;
    owner_primitive.ellipse.start_param = primitive.ellipse.start_param;
    owner_primitive.ellipse.end_param = primitive.ellipse.end_param;
    owner_primitive.ellipse.clockwise = primitive.ellipse.clockwise;
    owner_primitive.point.position = primitive.point.position;
    owner_primitive.contour.closed = primitive.contour.closed;
    owner_primitive.contour.elements.reserve(primitive.contour.elements.size());
    for (const auto& element : primitive.contour.elements) {
        Owner::ValueObjects::ContourElement owner_element;
        owner_element.type = static_cast<Owner::ValueObjects::ContourElementType>(element.type);
        owner_element.line = ToOwnerLine(element.line);
        owner_element.arc = ToOwnerArc(element.arc);
        owner_element.spline = ToOwnerSpline(element.spline);
        owner_primitive.contour.elements.push_back(std::move(owner_element));
    }
    return owner_primitive;
}

Contracts::LinePrimitive ToContractsLine(const Owner::ValueObjects::LinePrimitive& line) {
    return Contracts::LinePrimitive{line.start, line.end};
}

Contracts::ArcPrimitive ToContractsArc(const Owner::ValueObjects::ArcPrimitive& arc) {
    Contracts::ArcPrimitive contract_arc;
    contract_arc.center = arc.center;
    contract_arc.radius = arc.radius;
    contract_arc.start_angle_deg = arc.start_angle_deg;
    contract_arc.end_angle_deg = arc.end_angle_deg;
    contract_arc.clockwise = arc.clockwise;
    return contract_arc;
}

Contracts::SplinePrimitive ToContractsSpline(const Owner::ValueObjects::SplinePrimitive& spline) {
    Contracts::SplinePrimitive contract_spline;
    contract_spline.control_points = spline.control_points;
    contract_spline.closed = spline.closed;
    return contract_spline;
}

Contracts::Segment ToContractsSegment(const Owner::ValueObjects::Segment& segment) {
    Contracts::Segment contract_segment;
    contract_segment.type = static_cast<Contracts::SegmentType>(segment.type);
    contract_segment.line = ToContractsLine(segment.line);
    contract_segment.arc = ToContractsArc(segment.arc);
    contract_segment.spline = ToContractsSpline(segment.spline);
    contract_segment.length = segment.length;
    contract_segment.curvature_radius = segment.curvature_radius;
    contract_segment.is_spline_approx = segment.is_spline_approx;
    contract_segment.is_point = segment.is_point;
    return contract_segment;
}

Contracts::Path ToContractsPath(const Owner::ValueObjects::Path& path) {
    Contracts::Path contract_path;
    contract_path.closed = path.closed;
    contract_path.segments.reserve(path.segments.size());
    for (const auto& segment : path.segments) {
        contract_path.segments.push_back(ToContractsSegment(segment));
    }
    return contract_path;
}

Contracts::ProcessConstraint ToContractsConstraint(const Owner::ValueObjects::ProcessConstraint& constraint) {
    Contracts::ProcessConstraint contract_constraint;
    contract_constraint.has_velocity_factor = constraint.has_velocity_factor;
    contract_constraint.max_velocity_factor = constraint.max_velocity_factor;
    contract_constraint.has_local_speed_hint = constraint.has_local_speed_hint;
    contract_constraint.local_speed_factor = constraint.local_speed_factor;
    return contract_constraint;
}

Contracts::ProcessPath ToContractsProcessPath(const Owner::ValueObjects::ProcessPath& path) {
    Contracts::ProcessPath contract_path;
    contract_path.segments.reserve(path.segments.size());
    for (const auto& segment : path.segments) {
        Contracts::ProcessSegment contract_segment;
        contract_segment.geometry = ToContractsSegment(segment.geometry);
        contract_segment.dispense_on = segment.dispense_on;
        contract_segment.flow_rate = segment.flow_rate;
        contract_segment.constraint = ToContractsConstraint(segment.constraint);
        contract_segment.tag = static_cast<Contracts::ProcessTag>(segment.tag);
        contract_path.segments.push_back(std::move(contract_segment));
    }
    return contract_path;
}

void OptimizeLineOrientation(Siligen::ProcessPath::Contracts::Path& path,
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
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
    const UnifiedTrajectoryPlanRequest& request) const {
    UnifiedTrajectoryPlanResult result;
    std::vector<Owner::ValueObjects::Primitive> owner_primitives;
    owner_primitives.reserve(primitives.size());
    for (const auto& primitive : primitives) {
        owner_primitives.push_back(ToOwnerPrimitive(primitive));
    }

    Owner::DomainServices::NormalizationConfig normalization{};
    normalization.unit_scale = request.normalization.unit_scale;
    normalization.continuity_tolerance = request.normalization.continuity_tolerance;
    normalization.approximate_splines = request.normalization.approximate_splines;
    normalization.spline_max_step_mm = request.normalization.spline_max_step_mm;
    normalization.spline_max_error_mm = request.normalization.spline_max_error_mm;

    Owner::DomainServices::GeometryNormalizer normalizer;
    const auto normalized = normalizer.Normalize(owner_primitives, normalization);
    result.normalized.path = ToContractsPath(normalized.path);
    result.normalized.report.discontinuity_count = normalized.report.discontinuity_count;
    result.normalized.report.closed = normalized.report.closed;
    result.normalized.report.invalid_unit_scale = normalized.report.invalid_unit_scale;
    result.normalized.report.skipped_spline_count = normalized.report.skipped_spline_count;
    result.normalized.report.point_primitive_count = normalized.report.point_primitive_count;
    result.normalized.report.consumable_segment_count = normalized.report.consumable_segment_count;
    OptimizeLineOrientation(result.normalized.path, request.normalization.continuity_tolerance);

    Owner::ValueObjects::ProcessConfig process{};
    process.default_flow = request.process.default_flow;
    process.lead_on_dist = request.process.lead_on_dist;
    process.lead_off_dist = request.process.lead_off_dist;
    process.corner_slowdown = request.process.corner_slowdown;
    process.corner_angle_deg = request.process.corner_angle_deg;
    process.curve_chain_angle_deg = request.process.curve_chain_angle_deg;
    process.curve_chain_max_segment_mm = request.process.curve_chain_max_segment_mm;
    process.rapid_gap_threshold = request.process.rapid_gap_threshold;
    process.start_speed_factor = request.process.start_speed_factor;
    process.end_speed_factor = request.process.end_speed_factor;
    process.corner_speed_factor = request.process.corner_speed_factor;
    process.rapid_speed_factor = request.process.rapid_speed_factor;

    Owner::DomainServices::ProcessAnnotator annotator;
    const auto process_path = annotator.Annotate(normalized.path, process);
    result.process_path = ToContractsProcessPath(process_path);

    Owner::DomainServices::TrajectoryShaperConfig shaping{};
    shaping.corner_smoothing_radius = request.shaping.corner_smoothing_radius;
    shaping.corner_max_deviation_mm = request.shaping.corner_max_deviation_mm;
    shaping.corner_min_radius_mm = request.shaping.corner_min_radius_mm;
    shaping.position_tolerance = request.shaping.position_tolerance;

    Owner::DomainServices::TrajectoryShaper shaper;
    result.shaped_path = ToContractsProcessPath(shaper.Shape(process_path, shaping));

    if (request.generate_motion_trajectory) {
        Domain::Motion::DomainServices::MotionPlanner planner(velocity_service_);
        result.motion_trajectory = planner.Plan(result.shaped_path, request.motion);
    }

    return result;
}

}  // namespace Siligen::Domain::Dispensing::DomainServices


