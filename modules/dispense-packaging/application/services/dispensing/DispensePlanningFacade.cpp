#include "application/services/dispensing/DispensePlanningFacade.h"

#include "domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h"
#include "domain/dispensing/planning/domain-services/CurveFlatteningService.h"

#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/domain-services/TimeTrajectoryPlanner.h"
#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/TrajectoryTriggerUtils.h"
#include "shared/types/VisualizationTypes.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <sstream>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlanner;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlannerRequest;
using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;
using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState;
using Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Domain::Motion::CMPCoordinatedInterpolator;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::TrajectoryInterpolatorFactory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Motion::DomainServices::TimeTrajectoryPlanner;
using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentEnd;
using Siligen::Domain::Trajectory::ValueObjects::SegmentStart;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::MotionPlanning::Contracts::MotionPlan;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kGluePointDedupEpsilonMm = 1e-4f;

struct TriggerArtifacts {
    std::vector<float32> distances;
    std::vector<Point2D> positions;
    std::vector<AuthorityTriggerPoint> authority_trigger_points;
    std::vector<SpacingValidationGroup> spacing_validation_groups;
    AuthorityTriggerLayout authority_trigger_layout;
    uint32 interval_ms = 0;
    float32 interval_mm = 0.0f;
    bool downgraded = false;
    bool binding_ready = false;
    bool spacing_valid = false;
    bool has_short_segment_exceptions = false;
    std::string validation_classification;
    std::string exception_reason;
    std::string failure_reason;
};

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
    bool authority_shared = false;
};

struct TriggerBindingCandidate {
    bool found = false;
    std::size_t interpolation_index = 0;
    float32 distance_error_mm = std::numeric_limits<float32>::max();
    float32 match_error_mm = std::numeric_limits<float32>::max();
    bool monotonic = true;
};

bool IsBetterTriggerBindingCandidate(
    const TriggerBindingCandidate& candidate,
    const TriggerBindingCandidate& current) {
    if (!candidate.found) {
        return false;
    }
    if (!current.found) {
        return true;
    }

    if (candidate.distance_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.distance_error_mm) {
        return true;
    }
    if (std::fabs(candidate.distance_error_mm - current.distance_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.match_error_mm + Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm <
        current.match_error_mm) {
        return true;
    }
    if (std::fabs(candidate.match_error_mm - current.match_error_mm) >
        Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm) {
        return false;
    }

    if (candidate.monotonic != current.monotonic) {
        return candidate.monotonic;
    }

    return candidate.interpolation_index < current.interpolation_index;
}

float32 SegmentLength(const Segment& segment) {
    if (segment.length > kEpsilon) {
        return segment.length;
    }

    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.start.DistanceTo(segment.line.end);
        case SegmentType::Arc:
            return ComputeArcLength(segment.arc);
        case SegmentType::Spline: {
            if (segment.spline.control_points.size() < 2) {
                return 0.0f;
            }
            float32 total = 0.0f;
            for (size_t i = 1; i < segment.spline.control_points.size(); ++i) {
                total += segment.spline.control_points[i - 1].DistanceTo(segment.spline.control_points[i]);
            }
            return total;
        }
        default:
            return 0.0f;
    }
}

float32 ComputeProcessPathLength(const ProcessPath& process_path) {
    float32 total_length = 0.0f;
    for (const auto& segment : process_path.segments) {
        total_length += SegmentLength(segment.geometry);
    }
    return total_length;
}

bool PointsNear(const Point2D& lhs, const Point2D& rhs, float32 epsilon_mm) {
    return lhs.DistanceTo(rhs) <= epsilon_mm;
}

bool ContainsNearPoint(const std::vector<Point2D>& points, const Point2D& point, float32 epsilon_mm) {
    for (const auto& existing : points) {
        if (PointsNear(existing, point, epsilon_mm)) {
            return true;
        }
    }
    return false;
}

float32 ResolveSpacingMin(const PlanningArtifactsBuildInput& input, float32 target_spacing) {
    if (input.spacing_min_mm > 0.0f) {
        return input.spacing_min_mm;
    }
    (void)target_spacing;
    return 2.7f;
}

float32 ResolveSpacingMax(const PlanningArtifactsBuildInput& input, float32 target_spacing) {
    if (input.spacing_max_mm > 0.0f) {
        return input.spacing_max_mm;
    }
    (void)target_spacing;
    return 3.3f;
}

bool ResolveSplinePosition(const Segment& segment, float32 local_distance, Point2D& out) {
    if (segment.spline.control_points.empty()) {
        out = SegmentEnd(segment);
        return true;
    }
    if (segment.spline.control_points.size() == 1) {
        out = segment.spline.control_points.front();
        return true;
    }

    const float32 total_length = SegmentLength(segment);
    if (total_length <= kEpsilon) {
        out = segment.spline.control_points.back();
        return true;
    }

    float32 accumulated = 0.0f;
    for (size_t index = 1; index < segment.spline.control_points.size(); ++index) {
        const auto& start = segment.spline.control_points[index - 1];
        const auto& end = segment.spline.control_points[index];
        const float32 edge_length = start.DistanceTo(end);
        if (edge_length <= kEpsilon) {
            continue;
        }
        if (accumulated + edge_length >= local_distance - kEpsilon) {
            const float32 ratio = std::clamp((local_distance - accumulated) / edge_length, 0.0f, 1.0f);
            out = start + (end - start) * ratio;
            return true;
        }
        accumulated += edge_length;
    }

    out = segment.spline.control_points.back();
    return true;
}

bool ResolveSegmentPosition(const Segment& segment, float32 local_distance, Point2D& out) {
    const float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        out = SegmentEnd(segment);
        return true;
    }

    const float32 ratio = std::clamp(local_distance / segment_length, 0.0f, 1.0f);
    switch (segment.type) {
        case SegmentType::Line:
            out = segment.line.start + (segment.line.end - segment.line.start) * ratio;
            return true;
        case SegmentType::Arc: {
            const float32 sweep = ComputeArcSweep(segment.arc.start_angle_deg,
                                                  segment.arc.end_angle_deg,
                                                  segment.arc.clockwise);
            const float32 direction = segment.arc.clockwise ? -1.0f : 1.0f;
            const float32 angle = segment.arc.start_angle_deg + direction * sweep * ratio;
            out = ArcPoint(segment.arc, angle);
            return true;
        }
        case SegmentType::Spline:
            return ResolveSplinePosition(segment, local_distance, out);
        default:
            out = SegmentEnd(segment);
            return true;
    }
}

void AppendAuthorityPoint(TriggerArtifacts& artifacts, AuthorityTriggerPoint point) {
    if (!artifacts.authority_trigger_points.empty()) {
        auto& previous = artifacts.authority_trigger_points.back();
        if (PointsNear(previous.position, point.position, kGluePointDedupEpsilonMm) &&
            std::fabs(previous.trigger_distance_mm - point.trigger_distance_mm) <= kEpsilon) {
            previous.shared_vertex = true;
            return;
        }
        if (PointsNear(previous.position, point.position, kGluePointDedupEpsilonMm)) {
            previous.shared_vertex = true;
            point.shared_vertex = true;
        }
    }

    artifacts.distances.push_back(point.trigger_distance_mm);
    artifacts.positions.push_back(point.position);
    artifacts.authority_trigger_points.push_back(std::move(point));
}

Result<void> AppendAnchoredAuthorityForSegment(
    TriggerArtifacts& artifacts,
    const Segment& segment,
    std::size_t segment_index,
    float32 path_offset_mm,
    float32 target_spacing_mm,
    float32 spacing_min_mm,
    float32 spacing_max_mm) {
    const float32 segment_length = SegmentLength(segment);
    if (segment_length <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "dispense_on 几何线段长度为0", "DispensePlanningFacade"));
    }
    if (target_spacing_mm <= kEpsilon) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "trigger_spatial_interval_mm 无效", "DispensePlanningFacade"));
    }

    const int division_count = std::max(1, static_cast<int>(std::lround(segment_length / target_spacing_mm)));
    const float32 actual_spacing_mm = segment_length / static_cast<float32>(division_count);

    SpacingValidationGroup group;
    group.segment_index = segment_index;
    group.actual_spacing_mm = actual_spacing_mm;
    group.short_segment_exception =
        actual_spacing_mm < spacing_min_mm - kEpsilon || actual_spacing_mm > spacing_max_mm + kEpsilon;
    group.within_window =
        actual_spacing_mm >= spacing_min_mm - kEpsilon && actual_spacing_mm <= spacing_max_mm + kEpsilon;

    group.points.reserve(static_cast<std::size_t>(division_count) + 1U);
    for (int division = 0; division <= division_count; ++division) {
        const float32 local_distance =
            (division == division_count) ? segment_length : actual_spacing_mm * static_cast<float32>(division);
        Point2D point;
        if (!ResolveSegmentPosition(segment, local_distance, point)) {
            return Result<void>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "authority trigger 位置解析失败", "DispensePlanningFacade"));
        }
        group.points.push_back(point);

        AuthorityTriggerPoint authority_point;
        authority_point.position = point;
        authority_point.trigger_distance_mm = path_offset_mm + local_distance;
        authority_point.segment_index = segment_index;
        authority_point.short_segment_exception = group.short_segment_exception;
        AppendAuthorityPoint(artifacts, std::move(authority_point));
    }

    artifacts.spacing_validation_groups.push_back(std::move(group));
    return Result<void>::Success();
}

std::vector<VisualizationTriggerPoint> BuildPlannedTriggerPoints(
    const std::vector<TrajectoryPoint>& interpolation_points) {
    std::vector<VisualizationTriggerPoint> triggers;
    if (interpolation_points.empty()) {
        return triggers;
    }

    std::vector<Point2D> preview_positions;
    const float32 nan_value = std::numeric_limits<float32>::quiet_NaN();
    for (const auto& point : interpolation_points) {
        if (!point.enable_position_trigger) {
            continue;
        }
        if (ContainsNearPoint(preview_positions, point.position, kGluePointDedupEpsilonMm)) {
            continue;
        }
        preview_positions.push_back(point.position);
        triggers.emplace_back(point.position, nan_value);
    }
    return triggers;
}

std::vector<TrajectoryPoint> BuildTrajectoryFromMotion(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> base;
    if (trajectory.points.empty()) {
        return base;
    }

    base.reserve(trajectory.points.size());
    for (const auto& pt : trajectory.points) {
        TrajectoryPoint out;
        out.position = Siligen::Shared::Types::Point2D(pt.position);
        out.velocity = std::sqrt(pt.velocity.x * pt.velocity.x + pt.velocity.y * pt.velocity.y);
        out.timestamp = pt.t;
        // MotionTrajectory::dispense_on only means "glue is enabled on this path region".
        // Preview glue points must be reconstructed from trigger_distances_mm instead.
        out.enable_position_trigger = false;
        base.push_back(out);
    }
    return base;
}

ExecutionTrajectorySelection SelectExecutionTrajectory(
    const ExecutionPackageValidated& execution_package) {
    ExecutionTrajectorySelection selection;
    selection.motion_trajectory_points = BuildTrajectoryFromMotion(execution_package.execution_plan.motion_trajectory);
    if (!execution_package.execution_plan.interpolation_points.empty()) {
        selection.execution_trajectory = &execution_package.execution_plan.interpolation_points;
        selection.authority_shared = true;
        return selection;
    }
    return selection;
}

std::vector<Siligen::Shared::Types::Point2D> CollectTriggerPositions(
    const std::vector<VisualizationTriggerPoint>& preview_triggers) {
    std::vector<Siligen::Shared::Types::Point2D> final_glue_points;
    final_glue_points.reserve(preview_triggers.size());
    for (const auto& trigger : preview_triggers) {
        final_glue_points.push_back(trigger.position);
    }
    return final_glue_points;
}

const ProcessPath& ResolveAuthorityProcessPath(const PlanningArtifactsBuildInput& input) {
    if (!input.authority_process_path.segments.empty()) {
        return input.authority_process_path;
    }
    return input.process_path;
}

bool ValidateGlueSpacing(
    const PlanningArtifactsBuildInput& input,
    TriggerArtifacts& artifacts,
    float32 target_spacing_mm) {
    if (artifacts.spacing_validation_groups.empty()) {
        artifacts.spacing_valid = false;
        if (artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "authority spacing groups unavailable";
        }
        return false;
    }

    const float32 min_allowed = ResolveSpacingMin(input, target_spacing_mm);
    const float32 max_allowed = ResolveSpacingMax(input, target_spacing_mm);

    bool valid = true;
    std::size_t invalid_group_count = 0;
    std::size_t short_exception_count = 0;
    for (const auto& group : artifacts.spacing_validation_groups) {
        if (group.short_segment_exception) {
            ++short_exception_count;
            continue;
        }
        if (!group.within_window) {
            valid = false;
            ++invalid_group_count;
        }
    }

    artifacts.has_short_segment_exceptions = short_exception_count > 0;
    artifacts.spacing_valid = valid;
    if (!valid && artifacts.failure_reason.empty()) {
        std::ostringstream oss;
        oss << "spacing validation failed: invalid_groups=" << invalid_group_count
            << ", exceptions=" << short_exception_count
            << ", target=" << target_spacing_mm
            << ", window=[" << min_allowed << ',' << max_allowed << ']';
        artifacts.failure_reason = oss.str();
        SILIGEN_LOG_WARNING(artifacts.failure_reason);
    }
    return valid;
}

float32 EstimateExecutionTime(
    const PlanningArtifactsBuildInput& input,
    const ExecutionPackageBuilt& built) {
    if (input.estimated_time_s > kEpsilon) {
        return input.estimated_time_s;
    }
    if (built.estimated_time_s > kEpsilon) {
        return built.estimated_time_s;
    }
    if (input.motion_plan.total_time > kEpsilon) {
        return input.motion_plan.total_time;
    }
    if (built.total_length_mm > kEpsilon && input.dispensing_velocity > kEpsilon) {
        return built.total_length_mm / input.dispensing_velocity;
    }
    return 0.0f;
}

PlanningArtifactExportRequest BuildExportRequest(
    const PlanningArtifactsBuildInput& input,
    const ExecutionPackageValidated& execution_package,
    const ExecutionTrajectorySelection& selection,
    const std::vector<Siligen::Shared::Types::Point2D>& glue_points) {
    PlanningArtifactExportRequest export_request;
    export_request.source_path = input.source_path;
    export_request.dxf_filename = input.dxf_filename;
    export_request.process_path = input.process_path;
    export_request.glue_points = glue_points;
    if (selection.execution_trajectory) {
        export_request.execution_trajectory_points = *selection.execution_trajectory;
    }
    export_request.interpolation_trajectory_points = execution_package.execution_plan.interpolation_points;
    export_request.motion_trajectory_points = selection.motion_trajectory_points;
    return export_request;
}

float32 ResolveInterpolationStep(const PlanningArtifactsBuildInput& input) {
    if (input.sample_ds > kEpsilon) {
        return input.sample_ds;
    }
    if (input.spline_max_step_mm > kEpsilon) {
        return input.spline_max_step_mm;
    }
    return 1.0f;
}

float32 SpeedMagnitude(const MotionTrajectoryPoint& point) {
    const float32 vx = point.velocity.x;
    const float32 vy = point.velocity.y;
    return std::sqrt(vx * vx + vy * vy);
}

Siligen::Domain::Motion::ValueObjects::TimePlanningConfig BuildInterpolationPlanningConfig(
    const Siligen::InterpolationConfig& config) {
    Siligen::Domain::Motion::ValueObjects::TimePlanningConfig motion_config;
    motion_config.vmax = config.max_velocity;
    motion_config.amax = config.max_acceleration;
    motion_config.jmax = config.max_jerk;
    motion_config.sample_dt = config.time_step;
    motion_config.sample_ds = 0.0f;
    motion_config.curvature_speed_factor = config.curvature_speed_factor;
    motion_config.corner_speed_factor = 1.0f;
    motion_config.start_speed_factor = 1.0f;
    motion_config.end_speed_factor = 1.0f;
    motion_config.rapid_speed_factor = 1.0f;
    motion_config.arc_tolerance_mm = std::max(config.position_tolerance, 0.0f);
    motion_config.enforce_jerk_limit = true;
    return motion_config;
}

std::vector<TrajectoryPoint> ConvertMotionTrajectoryToTrajectoryPoints(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> points;
    points.reserve(trajectory.points.size());
    for (size_t index = 0; index < trajectory.points.size(); ++index) {
        const auto& sample = trajectory.points[index];
        TrajectoryPoint point;
        point.position = Point2D(sample.position.x, sample.position.y);
        point.timestamp = sample.t;
        point.sequence_id = static_cast<uint32>(index);
        point.velocity = SpeedMagnitude(sample);
        points.push_back(point);
    }
    return points;
}

void AppendUniquePoint(std::vector<Point2D>& points, const Point2D& point) {
    if (points.empty() || points.back().DistanceTo(point) > kEpsilon) {
        points.push_back(point);
    }
}

void SampleLinePoints(
    const Point2D& start,
    const Point2D& end,
    float32 step,
    std::vector<Point2D>& points) {
    const float32 length = start.DistanceTo(end);
    if (length <= kEpsilon) {
        return;
    }

    if (step <= kEpsilon || length <= step) {
        AppendUniquePoint(points, start);
        AppendUniquePoint(points, end);
        return;
    }

    int slices = static_cast<int>(std::ceil(length / step));
    slices = std::max(1, slices);
    for (int i = 0; i <= slices; ++i) {
        const float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        AppendUniquePoint(points, start + (end - start) * ratio);
    }
}

void SampleArcPoints(
    const Siligen::Domain::Trajectory::ValueObjects::ArcPrimitive& arc,
    float32 step,
    std::vector<Point2D>& points) {
    const float32 length = ComputeArcLength(arc);
    if (length <= kEpsilon) {
        return;
    }

    float32 sweep = ComputeArcSweep(arc.start_angle_deg, arc.end_angle_deg, arc.clockwise);
    int slices = 1;
    if (step > kEpsilon) {
        slices = static_cast<int>(std::ceil(length / step));
        slices = std::max(1, slices);
    }

    const float32 direction = arc.clockwise ? -1.0f : 1.0f;
    for (int i = 0; i <= slices; ++i) {
        const float32 ratio = static_cast<float32>(i) / static_cast<float32>(slices);
        const float32 angle = arc.start_angle_deg + direction * sweep * ratio;
        AppendUniquePoint(points, ArcPoint(arc, angle));
    }
}

Result<std::vector<Point2D>> BuildInterpolationSeedPoints(
    const ProcessPath& path,
    float32 spline_max_error_mm,
    float32 step_mm) {
    std::vector<Point2D> points;
    Siligen::Domain::Dispensing::DomainServices::CurveFlatteningService flattening_service;
    const float32 sample_step_mm = step_mm > kEpsilon ? step_mm : 1.0f;
    for (const auto& segment : path.segments) {
        const auto& geometry = segment.geometry;
        if (geometry.is_point) {
            AppendUniquePoint(points, geometry.line.start);
            continue;
        }

        auto flattened_result = flattening_service.Flatten(geometry, spline_max_error_mm, step_mm);
        if (flattened_result.IsError()) {
            return Result<std::vector<Point2D>>::Failure(flattened_result.GetError());
        }
        const auto& flattened_points = flattened_result.Value().points;
        if (flattened_points.empty()) {
            continue;
        }
        AppendUniquePoint(points, flattened_points.front());
        for (std::size_t index = 1; index < flattened_points.size(); ++index) {
            SampleLinePoints(flattened_points[index - 1], flattened_points[index], sample_step_mm, points);
        }
    }
    return Result<std::vector<Point2D>>::Success(std::move(points));
}

std::vector<AuthorityTriggerPoint> ConvertAuthorityTriggerPoints(const AuthorityTriggerLayout& layout) {
    std::vector<AuthorityTriggerPoint> points;
    points.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        AuthorityTriggerPoint point;
        point.position = trigger.position;
        point.trigger_distance_mm = trigger.distance_mm_global;
        point.segment_index = trigger.source_segment_index;
        point.short_segment_exception = false;
        point.shared_vertex = trigger.shared_vertex;
        points.push_back(point);
    }
    return points;
}

std::vector<SpacingValidationGroup> ConvertSpacingValidationGroups(const AuthorityTriggerLayout& layout) {
    std::vector<SpacingValidationGroup> groups;
    groups.reserve(layout.validation_outcomes.size());
    for (const auto& outcome : layout.validation_outcomes) {
        SpacingValidationGroup group;
        auto span_it = std::find_if(
            layout.spans.begin(),
            layout.spans.end(),
            [&](const auto& span) { return span.span_id == outcome.span_ref; });
        if (span_it != layout.spans.end() && !span_it->source_segment_indices.empty()) {
            group.segment_index = span_it->source_segment_indices.front();
            group.actual_spacing_mm = span_it->actual_spacing_mm;
        }
        group.points = outcome.points;
        group.short_segment_exception =
            outcome.classification == SpacingValidationClassification::PassWithException;
        group.within_window = outcome.classification == SpacingValidationClassification::Pass;
        groups.push_back(std::move(group));
    }
    return groups;
}

std::string ResolveValidationClassification(const AuthorityTriggerLayout& layout) {
    if (layout.validation_outcomes.empty()) {
        return layout.authority_ready ? "pass" : "fail";
    }
    bool has_exception = false;
    for (const auto& outcome : layout.validation_outcomes) {
        if (outcome.classification == SpacingValidationClassification::Fail) {
            return "fail";
        }
        if (outcome.classification == SpacingValidationClassification::PassWithException) {
            has_exception = true;
        }
    }
    return has_exception ? "pass_with_exception" : "pass";
}

std::string ResolveExceptionReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.exception_reason.empty()) {
            return outcome.exception_reason;
        }
    }
    return {};
}

std::string ResolveBlockingReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.blocking_reason.empty()) {
            return outcome.blocking_reason;
        }
    }
    return {};
}

std::vector<Point2D> CollectAuthorityPositions(const AuthorityTriggerLayout& layout) {
    std::vector<Point2D> positions;
    positions.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        positions.push_back(trigger.position);
    }
    return positions;
}

Result<TriggerArtifacts> BuildTriggerArtifacts(
    const ProcessPath& path,
    const PlanningArtifactsBuildInput& input) {
    TriggerArtifacts artifacts;
    if (path.segments.empty()) {
        artifacts.failure_reason = "process path is empty";
        return Result<TriggerArtifacts>::Success(std::move(artifacts));
    }

    AuthorityTriggerLayoutPlanner planner;
    AuthorityTriggerLayoutPlannerRequest request;
    request.process_path = path;
    request.plan_id = input.source_path;
    request.plan_fingerprint = input.dxf_filename;
    request.layout_id_seed = input.source_path + "|" + input.dxf_filename + "|" + std::to_string(path.segments.size());
    request.target_spacing_mm = input.trigger_spatial_interval_mm;
    request.min_spacing_mm = ResolveSpacingMin(input, input.trigger_spatial_interval_mm);
    request.max_spacing_mm = ResolveSpacingMax(input, input.trigger_spatial_interval_mm);
    request.dispensing_velocity = input.dispensing_velocity;
    request.acceleration = input.acceleration;
    request.dispenser_interval_ms = input.dispenser_interval_ms;
    request.dispenser_duration_ms = input.dispenser_duration_ms;
    request.valve_response_ms = input.valve_response_ms;
    request.safety_margin_ms = input.safety_margin_ms;
    request.min_interval_ms = input.min_interval_ms;
    request.downgrade_on_violation = input.downgrade_on_violation;
    request.dispensing_strategy = input.dispensing_strategy;
    request.subsegment_count = input.subsegment_count;
    request.dispense_only_cruise = input.dispense_only_cruise;
    request.compensation_profile = input.compensation_profile;
    request.spline_max_error_mm = input.spline_max_error_mm;
    request.spline_max_step_mm = input.spline_max_step_mm;

    auto layout_result = planner.Plan(request);
    if (layout_result.IsError()) {
        return Result<TriggerArtifacts>::Failure(layout_result.GetError());
    }

    artifacts.authority_trigger_layout = layout_result.Value();
    artifacts.authority_trigger_points = ConvertAuthorityTriggerPoints(artifacts.authority_trigger_layout);
    artifacts.spacing_validation_groups = ConvertSpacingValidationGroups(artifacts.authority_trigger_layout);
    artifacts.positions = CollectAuthorityPositions(artifacts.authority_trigger_layout);
    artifacts.validation_classification = ResolveValidationClassification(artifacts.authority_trigger_layout);
    artifacts.exception_reason = ResolveExceptionReason(artifacts.authority_trigger_layout);
    artifacts.failure_reason = ResolveBlockingReason(artifacts.authority_trigger_layout);

    for (const auto& trigger : artifacts.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }

    TriggerPlan trigger_plan;
    trigger_plan.strategy = input.dispensing_strategy;
    trigger_plan.interval_mm = input.trigger_spatial_interval_mm;
    trigger_plan.subsegment_count = input.subsegment_count;
    trigger_plan.dispense_only_cruise = input.dispense_only_cruise;
    trigger_plan.safety.duration_ms = static_cast<int32>(input.dispenser_duration_ms);
    trigger_plan.safety.valve_response_ms = static_cast<int32>(input.valve_response_ms);
    trigger_plan.safety.margin_ms = static_cast<int32>(input.safety_margin_ms);
    trigger_plan.safety.min_interval_ms = static_cast<int32>(input.min_interval_ms);
    trigger_plan.safety.downgrade_on_violation = input.downgrade_on_violation;

    TriggerPlanner trigger_planner;
    for (const auto& span : artifacts.authority_trigger_layout.spans) {
        auto timing_result = trigger_planner.Plan(
            span.total_length_mm,
            input.dispensing_velocity,
            input.acceleration,
            input.trigger_spatial_interval_mm,
            input.dispenser_interval_ms,
            0.0f,
            trigger_plan,
            input.compensation_profile);
        if (timing_result.IsError()) {
            return Result<TriggerArtifacts>::Failure(timing_result.GetError());
        }
        const auto& trigger_result = timing_result.Value();
        artifacts.interval_ms = std::max<uint32>(artifacts.interval_ms, trigger_result.interval_ms);
        artifacts.interval_mm = std::max(artifacts.interval_mm, trigger_result.plan.interval_mm);
        artifacts.downgraded = artifacts.downgraded || trigger_result.downgrade_applied;
    }

    if (artifacts.interval_mm <= kEpsilon) {
        artifacts.interval_mm = input.trigger_spatial_interval_mm;
    }
    if (artifacts.interval_ms == 0) {
        artifacts.interval_ms = input.dispenser_interval_ms;
    }
    if (artifacts.authority_trigger_points.empty() && artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "explicit trigger authority unavailable";
    }
    ValidateGlueSpacing(input, artifacts, artifacts.interval_mm);
    return Result<TriggerArtifacts>::Success(std::move(artifacts));
}

Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints(
    const PlanningArtifactsBuildInput& input,
    const ProcessPath& path,
    const TriggerArtifacts& trigger_artifacts) {
    if (!input.use_interpolation_planner) {
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    if (trigger_artifacts.validation_classification == "fail") {
        return Result<std::vector<TrajectoryPoint>>::Success({});
    }

    const auto seed_points_result = BuildInterpolationSeedPoints(
        path,
        input.spline_max_error_mm,
        ResolveInterpolationStep(input));
    if (seed_points_result.IsError()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(seed_points_result.GetError());
    }
    const auto& seed_points = seed_points_result.Value();
    if (seed_points.size() < 2) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
    }

    Siligen::InterpolationConfig config{};
    config.max_velocity = input.dispensing_velocity;
    config.max_acceleration = input.acceleration;
    config.max_jerk = input.max_jerk;
    config.time_step = (input.sample_dt > kEpsilon) ? input.sample_dt : 0.001f;
    if (input.spline_max_error_mm > kEpsilon) {
        config.position_tolerance = input.spline_max_error_mm;
    }
    if (input.compensation_profile.curvature_speed_factor > kEpsilon) {
        config.curvature_speed_factor = input.compensation_profile.curvature_speed_factor;
    }
    if (input.trigger_spatial_interval_mm > kEpsilon) {
        config.trigger_spacing_mm = input.trigger_spatial_interval_mm;
    }

    if (config.max_velocity <= 0.0f ||
        config.max_acceleration <= 0.0f ||
        config.time_step <= 0.0f ||
        config.trigger_spacing_mm < 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
    }

    if ((input.interpolation_algorithm == InterpolationAlgorithm::LINEAR ||
         input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) &&
        config.max_jerk <= 0.0f) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
    }

    std::vector<TrajectoryPoint> points;
    if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED &&
        !trigger_artifacts.positions.empty()) {
        Siligen::CMPConfiguration cmp_config;
        cmp_config.trigger_mode = Siligen::CMPTriggerMode::POSITION_SYNC;
        cmp_config.cmp_channel = 1;
        cmp_config.pulse_width_us = 2000;
        cmp_config.trigger_position_tolerance = 0.1f;
        cmp_config.time_tolerance_ms = 1.0f;
        cmp_config.enable_compensation = true;
        cmp_config.compensation_factor = 1.0f;
        cmp_config.enable_multi_channel = false;

        std::vector<Siligen::DispensingTriggerPoint> triggers;
        triggers.reserve(trigger_artifacts.positions.size());
        for (std::size_t index = 0; index < trigger_artifacts.positions.size(); ++index) {
            Siligen::DispensingTriggerPoint trigger;
            trigger.position = trigger_artifacts.positions[index];
            trigger.trigger_distance = trigger_artifacts.distances[index];
            trigger.sequence_id = static_cast<uint32>(index);
            trigger.pulse_width_us = cmp_config.pulse_width_us;
            trigger.is_enabled = true;
            triggers.push_back(trigger);
        }

        CMPCoordinatedInterpolator cmp_interpolator;
        points = cmp_interpolator.PositionTriggeredDispensing(path, triggers, cmp_config, config);
    } else {
        if (input.interpolation_algorithm == InterpolationAlgorithm::CMP_COORDINATED) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "CMP插补缺少显式 trigger authority，不能退化为默认轨迹采样触发",
                      "DispensePlanningFacade"));
        }

        if (input.interpolation_algorithm == InterpolationAlgorithm::LINEAR) {
            TimeTrajectoryPlanner trajectory_planner;
            points = ConvertMotionTrajectoryToTrajectoryPoints(
                trajectory_planner.Plan(path, BuildInterpolationPlanningConfig(config)));
        } else {
            auto interpolator = TrajectoryInterpolatorFactory::CreateInterpolator(input.interpolation_algorithm);
            if (!interpolator) {
                return Result<std::vector<TrajectoryPoint>>::Failure(
                    Error(ErrorCode::NOT_IMPLEMENTED, "插补算法未实现", "DispensePlanningFacade"));
            }
            if (!interpolator->ValidateParameters(seed_points, config)) {
                return Result<std::vector<TrajectoryPoint>>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "插补规划参数无效", "DispensePlanningFacade"));
            }
            points = interpolator->CalculateInterpolation(seed_points, config);
        }

        if (!trigger_artifacts.positions.empty() &&
            !Siligen::Shared::Types::ApplyTriggerMarkersByPosition(
                points,
                trigger_artifacts.positions,
                trigger_artifacts.distances,
                Siligen::Shared::Types::kTriggerMarkerDedupToleranceMm,
                std::max(config.position_tolerance, Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm))) {
            return Result<std::vector<TrajectoryPoint>>::Failure(
                Error(ErrorCode::TRAJECTORY_GENERATION_FAILED,
                      "显式 trigger authority 映射到插补轨迹失败",
                      "DispensePlanningFacade"));
        }
    }

    if (points.empty()) {
        return Result<std::vector<TrajectoryPoint>>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "插补结果为空", "DispensePlanningFacade"));
    }

    return Result<std::vector<TrajectoryPoint>>::Success(std::move(points));
}

void BindAuthorityLayoutToExecutionTrajectory(
    TriggerArtifacts& artifacts,
    const std::vector<TrajectoryPoint>* execution_trajectory) {
    artifacts.authority_trigger_layout.bindings.clear();
    artifacts.binding_ready = false;
    artifacts.authority_trigger_layout.binding_ready = false;

    if (execution_trajectory == nullptr || artifacts.authority_trigger_layout.trigger_points.empty()) {
        return;
    }

    std::size_t last_interpolation_index = 0;
    std::vector<bool> consumed(execution_trajectory->size(), false);
    for (std::size_t trigger_index = 0;
         trigger_index < artifacts.authority_trigger_layout.trigger_points.size();
         ++trigger_index) {
        const auto& trigger = artifacts.authority_trigger_layout.trigger_points[trigger_index];
        TriggerBindingCandidate best_candidate;
        for (std::size_t interpolation_index = 0;
             interpolation_index < execution_trajectory->size();
             ++interpolation_index) {
            if (consumed[interpolation_index]) {
                continue;
            }

            const auto& trajectory_point = (*execution_trajectory)[interpolation_index];
            if (!trajectory_point.enable_position_trigger) {
                continue;
            }

            const float32 distance_error_mm =
                std::fabs(trajectory_point.trigger_position_mm - trigger.distance_mm_global);
            const float32 match_error_mm = trajectory_point.position.DistanceTo(trigger.position);
            if (distance_error_mm > Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm ||
                match_error_mm > std::max(
                    kGluePointDedupEpsilonMm,
                    Siligen::Shared::Types::kTriggerMarkerMatchToleranceMm)) {
                continue;
            }

            TriggerBindingCandidate candidate;
            candidate.found = true;
            candidate.interpolation_index = interpolation_index;
            candidate.distance_error_mm = distance_error_mm;
            candidate.match_error_mm = match_error_mm;
            candidate.monotonic = interpolation_index >= last_interpolation_index;
            if (IsBetterTriggerBindingCandidate(candidate, best_candidate)) {
                best_candidate = candidate;
            }
        }

        if (!best_candidate.found) {
            artifacts.failure_reason = "authority trigger binding unavailable";
            return;
        }

        const auto& trajectory_point = (*execution_trajectory)[best_candidate.interpolation_index];
        InterpolationTriggerBinding binding;
        binding.binding_id = artifacts.authority_trigger_layout.layout_id + "-binding-" + std::to_string(trigger_index);
        binding.layout_ref = artifacts.authority_trigger_layout.layout_id;
        binding.trigger_ref = trigger.trigger_id;
        binding.interpolation_index = best_candidate.interpolation_index;
        binding.execution_position = trajectory_point.position;
        binding.match_error_mm = best_candidate.match_error_mm;
        binding.monotonic = best_candidate.monotonic;
        binding.bound = true;

        artifacts.authority_trigger_layout.bindings.push_back(binding);
        consumed[best_candidate.interpolation_index] = true;
        last_interpolation_index = best_candidate.interpolation_index;
    }

    artifacts.binding_ready =
        artifacts.authority_trigger_layout.bindings.size() == artifacts.authority_trigger_layout.trigger_points.size();
    artifacts.authority_trigger_layout.binding_ready = artifacts.binding_ready;
    if (artifacts.binding_ready) {
        artifacts.authority_trigger_layout.state = AuthorityTriggerLayoutState::BindingReady;
    } else if (artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "authority trigger binding unavailable";
    }
}

}  // namespace

Result<PlanningArtifactsBuildResult> DispensePlanningFacade::AssemblePlanningArtifacts(
    const PlanningArtifactsBuildInput& input) const {
    if (input.process_path.segments.empty()) {
        return Result<PlanningArtifactsBuildResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path为空", "DispensePlanningFacade"));
    }
    if (input.motion_plan.points.empty()) {
        return Result<PlanningArtifactsBuildResult>::Failure(
            Error(ErrorCode::TRAJECTORY_GENERATION_FAILED, "motion plan为空", "DispensePlanningFacade"));
    }

    const auto& authority_process_path = ResolveAuthorityProcessPath(input);

    auto trigger_artifacts_result = BuildTriggerArtifacts(authority_process_path, input);
    if (trigger_artifacts_result.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(trigger_artifacts_result.GetError());
    }
    auto trigger_artifacts = trigger_artifacts_result.Value();

    auto interpolation_points_result =
        BuildInterpolationPoints(input, input.process_path, trigger_artifacts);
    if (interpolation_points_result.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(interpolation_points_result.GetError());
    }
    auto interpolation_points = interpolation_points_result.Value();

    Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner program_planner;
    auto interpolation_program =
        program_planner.BuildProgram(input.process_path, input.motion_plan, input.acceleration);
    if (interpolation_program.IsError() && trigger_artifacts.validation_classification != "fail") {
        return Result<PlanningArtifactsBuildResult>::Failure(interpolation_program.GetError());
    }

    ExecutionPackageBuilt built;
    if (interpolation_program.IsSuccess()) {
        built.execution_plan.interpolation_segments = std::move(interpolation_program.Value());
    }
    built.execution_plan.interpolation_points = std::move(interpolation_points);
    built.execution_plan.motion_trajectory = input.motion_plan;
    built.execution_plan.trigger_distances_mm = trigger_artifacts.distances;
    built.execution_plan.trigger_interval_ms = trigger_artifacts.interval_ms;
    built.execution_plan.trigger_interval_mm = trigger_artifacts.interval_mm;
    built.execution_plan.total_length_mm =
        input.motion_plan.total_length > kEpsilon ? input.motion_plan.total_length : ComputeProcessPathLength(input.process_path);
    built.total_length_mm = built.execution_plan.total_length_mm;
    built.estimated_time_s = input.estimated_time_s;
    built.source_path = input.source_path;

    built.estimated_time_s = EstimateExecutionTime(input, built);

    ExecutionPackageValidated execution_package(std::move(built));
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<PlanningArtifactsBuildResult>::Failure(package_validation.GetError());
    }

    const auto selection = SelectExecutionTrajectory(execution_package);
    BindAuthorityLayoutToExecutionTrajectory(trigger_artifacts, selection.execution_trajectory);
    const auto glue_points = CollectAuthorityPositions(trigger_artifacts.authority_trigger_layout);

    PlanningArtifactsBuildResult result;
    result.execution_package = execution_package;
    result.segment_count = static_cast<int>(input.process_path.segments.size());
    result.total_length = execution_package.total_length_mm;
    result.estimated_time = execution_package.estimated_time_s;
    if (selection.execution_trajectory) {
        result.trajectory_points = *selection.execution_trajectory;
    }
    result.glue_points = glue_points;
    result.trigger_count = static_cast<int>(glue_points.size());
    result.dxf_filename = input.dxf_filename;
    result.timestamp = static_cast<int32>(std::time(nullptr));
    result.planning_report = input.motion_plan.planning_report;
    result.preview_authority_ready = !trigger_artifacts.authority_trigger_points.empty() &&
                                     trigger_artifacts.binding_ready &&
                                     !glue_points.empty();
    result.preview_authority_shared_with_execution = trigger_artifacts.binding_ready;
    result.preview_binding_ready = trigger_artifacts.binding_ready;
    result.preview_spacing_valid = trigger_artifacts.spacing_valid;
    result.preview_has_short_segment_exceptions = trigger_artifacts.has_short_segment_exceptions;
    result.preview_validation_classification = trigger_artifacts.validation_classification;
    result.preview_exception_reason = trigger_artifacts.exception_reason;
    result.preview_failure_reason = trigger_artifacts.failure_reason;
    if (!result.preview_authority_ready && result.preview_failure_reason.empty()) {
        result.preview_failure_reason = trigger_artifacts.authority_trigger_points.empty()
            ? "explicit trigger authority unavailable"
            : "authority trigger binding unavailable";
    }
    result.authority_trigger_layout = trigger_artifacts.authority_trigger_layout;
    result.authority_trigger_points = trigger_artifacts.authority_trigger_points;
    result.spacing_validation_groups = trigger_artifacts.spacing_validation_groups;
    result.export_request = BuildExportRequest(input, execution_package, selection, glue_points);
    return Result<PlanningArtifactsBuildResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing





