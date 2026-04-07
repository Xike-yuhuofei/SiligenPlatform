#pragma once

#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/PlanningAssemblyTypes.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Siligen::Application::Services::Dispensing::TestFixtures {

using Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildInput;
using Siligen::Application::Services::Dispensing::AuthorityPreviewBuildResult;
using Siligen::Application::Services::Dispensing::ExecutionAssemblyBuildInput;
using Siligen::Application::Services::Dispensing::PlanningArtifactsBuildInput;
using Siligen::Application::Services::Dispensing::WorkflowAuthorityPreviewArtifacts;
using Siligen::Application::Services::Dispensing::WorkflowAuthorityPreviewRequest;
using Siligen::Application::Services::Dispensing::WorkflowExecutionAssemblyRequest;
using Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyRequest;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;

inline constexpr char kMixedExplicitBoundaryWithReorderedBranchFamily[] =
    "mixed_explicit_boundary_with_reordered_branch_family";

inline MotionTrajectoryPoint BuildMotionPoint(float t, float x, float y, bool dispense_on = true) {
    MotionTrajectoryPoint point;
    point.t = t;
    point.position = Siligen::Point3D(x, y, 0.0f);
    point.velocity = Siligen::Point3D(10.0f, 0.0f, 0.0f);
    point.dispense_on = dispense_on;
    return point;
}

inline ProcessSegment BuildLineSegment(const Point2D& start, const Point2D& end, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = start;
    segment.line.end = end;
    segment.length = start.DistanceTo(end);

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

inline ProcessSegment BuildSplineSegment(
    const std::vector<Point2D>& control_points,
    bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Spline;
    segment.spline.control_points = control_points;
    segment.length = 0.0f;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

inline ProcessSegment BuildPointSegment(const Point2D& point, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = point;
    segment.line.end = point;
    segment.length = 0.0f;
    segment.is_point = true;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

inline PlanningArtifactsBuildInput BuildPlanningInput() {
    PlanningArtifactsBuildInput input;
    input.source_path = "sample.pb";
    input.dxf_filename = "sample.pb";
    input.dispensing_velocity = 10.0f;
    input.acceleration = 200.0f;
    input.dispenser_interval_ms = 100;
    input.dispenser_duration_ms = 100;
    input.trigger_spatial_interval_mm = 5.0f;
    input.sample_dt = 0.01f;
    input.max_jerk = 5000.0f;
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.dispensing_strategy = DispensingStrategy::BASELINE;
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 10.0f;
    input.motion_plan.total_time = 1.0f;
    input.estimated_time_s = 1.25f;
    return input;
}

inline PlanningArtifactsBuildInput BuildInput() {
    return BuildPlanningInput();
}

inline WorkflowAuthorityPreviewRequest BuildWorkflowAuthorityPreviewInput(const PlanningArtifactsBuildInput& input) {
    WorkflowAuthorityPreviewRequest authority_input;
    authority_input.process_path = input.process_path;
    authority_input.authority_process_path = input.authority_process_path;
    authority_input.source_path = input.source_path;
    authority_input.dxf_filename = input.dxf_filename;
    authority_input.runtime_options.dispensing_velocity = input.dispensing_velocity;
    authority_input.runtime_options.acceleration = input.acceleration;
    authority_input.runtime_options.dispenser_interval_ms = input.dispenser_interval_ms;
    authority_input.runtime_options.dispenser_duration_ms = input.dispenser_duration_ms;
    authority_input.runtime_options.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    authority_input.runtime_options.valve_response_ms = input.valve_response_ms;
    authority_input.runtime_options.safety_margin_ms = input.safety_margin_ms;
    authority_input.runtime_options.min_interval_ms = input.min_interval_ms;
    authority_input.runtime_options.sample_dt = input.sample_dt;
    authority_input.runtime_options.sample_ds = input.sample_ds;
    authority_input.runtime_options.spline_max_step_mm = input.spline_max_step_mm;
    authority_input.runtime_options.spline_max_error_mm = input.spline_max_error_mm;
    authority_input.dispensing_strategy = input.dispensing_strategy;
    authority_input.subsegment_count = input.subsegment_count;
    authority_input.dispense_only_cruise = input.dispense_only_cruise;
    authority_input.downgrade_on_violation = input.downgrade_on_violation;
    authority_input.runtime_options.compensation_profile = input.compensation_profile;
    authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
    authority_input.spacing_min_mm = input.spacing_min_mm;
    authority_input.spacing_max_mm = input.spacing_max_mm;
    return authority_input;
}

inline AuthorityPreviewBuildInput BuildAuthorityPreviewInput(const PlanningArtifactsBuildInput& input) {
    AuthorityPreviewBuildInput authority_input;
    authority_input.process_path = input.process_path;
    authority_input.authority_process_path = input.authority_process_path;
    authority_input.source_path = input.source_path;
    authority_input.dxf_filename = input.dxf_filename;
    authority_input.dispensing_velocity = input.dispensing_velocity;
    authority_input.acceleration = input.acceleration;
    authority_input.dispenser_interval_ms = input.dispenser_interval_ms;
    authority_input.dispenser_duration_ms = input.dispenser_duration_ms;
    authority_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    authority_input.valve_response_ms = input.valve_response_ms;
    authority_input.safety_margin_ms = input.safety_margin_ms;
    authority_input.min_interval_ms = input.min_interval_ms;
    authority_input.sample_dt = input.sample_dt;
    authority_input.sample_ds = input.sample_ds;
    authority_input.spline_max_step_mm = input.spline_max_step_mm;
    authority_input.spline_max_error_mm = input.spline_max_error_mm;
    authority_input.dispensing_strategy = input.dispensing_strategy;
    authority_input.subsegment_count = input.subsegment_count;
    authority_input.dispense_only_cruise = input.dispense_only_cruise;
    authority_input.downgrade_on_violation = input.downgrade_on_violation;
    authority_input.compensation_profile = input.compensation_profile;
    authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
    authority_input.spacing_min_mm = input.spacing_min_mm;
    authority_input.spacing_max_mm = input.spacing_max_mm;
    return authority_input;
}

inline WorkflowExecutionAssemblyRequest BuildWorkflowExecutionInput(
    const PlanningArtifactsBuildInput& input,
    const WorkflowAuthorityPreviewArtifacts& authority_preview) {
    WorkflowExecutionAssemblyRequest execution_input;
    execution_input.process_path = input.process_path;
    execution_input.authority_process_path = input.authority_process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.runtime_options.dispensing_velocity = input.dispensing_velocity;
    execution_input.runtime_options.acceleration = input.acceleration;
    execution_input.runtime_options.dispenser_interval_ms = input.dispenser_interval_ms;
    execution_input.runtime_options.dispenser_duration_ms = input.dispenser_duration_ms;
    execution_input.runtime_options.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    execution_input.runtime_options.valve_response_ms = input.valve_response_ms;
    execution_input.runtime_options.safety_margin_ms = input.safety_margin_ms;
    execution_input.runtime_options.min_interval_ms = input.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.runtime_options.sample_dt = input.sample_dt;
    execution_input.runtime_options.sample_ds = input.sample_ds;
    execution_input.runtime_options.spline_max_step_mm = input.spline_max_step_mm;
    execution_input.runtime_options.spline_max_error_mm = input.spline_max_error_mm;
    execution_input.estimated_time_s = input.estimated_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.runtime_options.compensation_profile = input.compensation_profile;
    execution_input.authority_preview = authority_preview;
    return execution_input;
}

inline ExecutionAssemblyBuildInput BuildExecutionInput(
    const PlanningArtifactsBuildInput& input,
    const AuthorityPreviewBuildResult& authority_preview) {
    ExecutionAssemblyBuildInput execution_input;
    execution_input.process_path = input.process_path;
    execution_input.authority_process_path = input.authority_process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.dispensing_velocity = input.dispensing_velocity;
    execution_input.acceleration = input.acceleration;
    execution_input.dispenser_interval_ms = input.dispenser_interval_ms;
    execution_input.dispenser_duration_ms = input.dispenser_duration_ms;
    execution_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    execution_input.valve_response_ms = input.valve_response_ms;
    execution_input.safety_margin_ms = input.safety_margin_ms;
    execution_input.min_interval_ms = input.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.sample_dt = input.sample_dt;
    execution_input.sample_ds = input.sample_ds;
    execution_input.spline_max_step_mm = input.spline_max_step_mm;
    execution_input.spline_max_error_mm = input.spline_max_error_mm;
    execution_input.estimated_time_s = input.estimated_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.compensation_profile;
    execution_input.authority_preview = authority_preview;
    return execution_input;
}

inline WorkflowPlanningAssemblyRequest BuildWorkflowPlanningInput(const PlanningArtifactsBuildInput& input) {
    WorkflowPlanningAssemblyRequest workflow_input;
    workflow_input.authority_preview_request = BuildWorkflowAuthorityPreviewInput(input);
    workflow_input.motion_plan = input.motion_plan;
    workflow_input.max_jerk = input.max_jerk;
    workflow_input.estimated_time_s = input.estimated_time_s;
    workflow_input.use_interpolation_planner = input.use_interpolation_planner;
    workflow_input.interpolation_algorithm = input.interpolation_algorithm;
    return workflow_input;
}

inline WorkflowPlanningAssemblyRequest BuildWorkflowPlanningInput() {
    return BuildWorkflowPlanningInput(BuildPlanningInput());
}

inline PlanningArtifactsBuildInput BuildPolylineInput(
    const std::vector<Point2D>& polyline,
    float spacing_mm = 3.0f) {
    auto input = BuildPlanningInput();
    input.process_path.segments.clear();
    input.motion_plan.points.clear();
    input.trigger_spatial_interval_mm = spacing_mm;

    float total_length = 0.0f;
    float timestamp = 0.0f;
    for (std::size_t index = 1; index < polyline.size(); ++index) {
        input.process_path.segments.push_back(BuildLineSegment(polyline[index - 1], polyline[index]));
        if (index == 1) {
            input.motion_plan.points.push_back(
                BuildMotionPoint(timestamp, polyline[index - 1].x, polyline[index - 1].y));
        }
        total_length += polyline[index - 1].DistanceTo(polyline[index]);
        timestamp += 1.0f;
        input.motion_plan.points.push_back(BuildMotionPoint(timestamp, polyline[index].x, polyline[index].y));
    }
    input.motion_plan.total_length = total_length;
    input.motion_plan.total_time = std::max(1.0f, total_length / 10.0f);
    input.estimated_time_s = input.motion_plan.total_time;
    return input;
}

inline std::size_t CountPointsNear(
    const std::vector<Point2D>& points,
    const Point2D& target,
    float tolerance_mm) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.DistanceTo(target) <= tolerance_mm) {
            ++count;
        }
    }
    return count;
}

inline bool HasConsecutiveNearDuplicatePoints(
    const std::vector<Point2D>& points,
    float tolerance_mm) {
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index - 1].DistanceTo(points[index]) <= tolerance_mm) {
            return true;
        }
    }
    return false;
}

inline std::size_t CountTriggerMarkers(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            ++count;
        }
    }
    return count;
}

template <typename TSpan>
inline std::size_t CountAnchorRoles(const TSpan& span, StrongAnchorRole role) {
    return static_cast<std::size_t>(std::count_if(
        span.strong_anchors.begin(),
        span.strong_anchors.end(),
        [&](const auto& anchor) { return anchor.role == role; }));
}

}  // namespace Siligen::Application::Services::Dispensing::TestFixtures
