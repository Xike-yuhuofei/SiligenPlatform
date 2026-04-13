#include "application/services/dispensing/PlanningAssemblyResidualInternals.h"

#include "domain/dispensing/planning/domain-services/CurveFlatteningService.h"
#include "process_path/contracts/GeometryUtils.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace Siligen::Application::Services::Dispensing::Internal {
namespace {

WorkflowAuthorityTriggerPoint BuildWorkflowAuthorityTriggerPoint(const AuthorityTriggerPoint& input) {
    WorkflowAuthorityTriggerPoint point;
    point.position = input.position;
    point.trigger_distance_mm = input.trigger_distance_mm;
    point.segment_index = input.segment_index;
    point.short_segment_exception = input.short_segment_exception;
    point.shared_vertex = input.shared_vertex;
    return point;
}

WorkflowSpacingValidationGroup BuildWorkflowSpacingValidationGroup(const SpacingValidationGroup& input) {
    WorkflowSpacingValidationGroup group;
    group.segment_index = input.segment_index;
    group.points = input.points;
    group.actual_spacing_mm = input.actual_spacing_mm;
    group.short_segment_exception = input.short_segment_exception;
    group.within_window = input.within_window;
    return group;
}

AuthorityTriggerPoint BuildInternalAuthorityTriggerPoint(const WorkflowAuthorityTriggerPoint& input) {
    AuthorityTriggerPoint point;
    point.position = input.position;
    point.trigger_distance_mm = input.trigger_distance_mm;
    point.segment_index = input.segment_index;
    point.short_segment_exception = input.short_segment_exception;
    point.shared_vertex = input.shared_vertex;
    return point;
}

SpacingValidationGroup BuildInternalSpacingValidationGroup(const WorkflowSpacingValidationGroup& input) {
    SpacingValidationGroup group;
    group.segment_index = input.segment_index;
    group.points = input.points;
    group.actual_spacing_mm = input.actual_spacing_mm;
    group.short_segment_exception = input.short_segment_exception;
    group.within_window = input.within_window;
    return group;
}

float32 SpeedMagnitude(const MotionTrajectoryPoint& point) {
    const float32 vx = point.velocity.x;
    const float32 vy = point.velocity.y;
    return std::sqrt(vx * vx + vy * vy);
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

}  // namespace

AuthorityPreviewBuildInput BuildAuthorityPreviewBuildInput(const WorkflowAuthorityPreviewRequest& input) {
    AuthorityPreviewBuildInput authority_input;
    authority_input.process_path = input.process_path;
    authority_input.authority_process_path = input.authority_process_path;
    authority_input.source_path = input.source_path;
    authority_input.dxf_filename = input.dxf_filename;
    authority_input.dispensing_velocity = input.runtime_options.dispensing_velocity;
    authority_input.acceleration = input.runtime_options.acceleration;
    authority_input.dispenser_interval_ms = input.runtime_options.dispenser_interval_ms;
    authority_input.dispenser_duration_ms = input.runtime_options.dispenser_duration_ms;
    authority_input.trigger_spatial_interval_mm = input.runtime_options.trigger_spatial_interval_mm;
    authority_input.valve_response_ms = input.runtime_options.valve_response_ms;
    authority_input.safety_margin_ms = input.runtime_options.safety_margin_ms;
    authority_input.min_interval_ms = input.runtime_options.min_interval_ms;
    authority_input.sample_dt = input.runtime_options.sample_dt;
    authority_input.sample_ds = input.runtime_options.sample_ds;
    authority_input.spline_max_step_mm = input.runtime_options.spline_max_step_mm;
    authority_input.spline_max_error_mm = input.runtime_options.spline_max_error_mm;
    authority_input.dispensing_strategy = input.dispensing_strategy;
    authority_input.subsegment_count = input.subsegment_count;
    authority_input.dispense_only_cruise = input.dispense_only_cruise;
    authority_input.downgrade_on_violation = input.downgrade_on_violation;
    authority_input.compensation_profile = input.runtime_options.compensation_profile;
    authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
    authority_input.spacing_min_mm = input.spacing_min_mm;
    authority_input.spacing_max_mm = input.spacing_max_mm;
    return authority_input;
}

WorkflowAuthorityPreviewArtifacts BuildWorkflowAuthorityPreviewArtifacts(const AuthorityPreviewBuildResult& input) {
    WorkflowAuthorityPreviewArtifacts artifacts;
    artifacts.segment_count = input.segment_count;
    artifacts.total_length = input.total_length;
    artifacts.estimated_time = input.estimated_time;
    artifacts.preview_trajectory_points = input.preview_trajectory_points;
    artifacts.glue_points = input.glue_points;
    artifacts.trigger_count = input.trigger_count;
    artifacts.dxf_filename = input.dxf_filename;
    artifacts.timestamp = input.timestamp;
    artifacts.preview_authority_ready = input.preview_authority_ready;
    artifacts.preview_binding_ready = input.preview_binding_ready;
    artifacts.preview_spacing_valid = input.preview_spacing_valid;
    artifacts.preview_has_short_segment_exceptions = input.preview_has_short_segment_exceptions;
    artifacts.preview_validation_classification = input.preview_validation_classification;
    artifacts.preview_exception_reason = input.preview_exception_reason;
    artifacts.preview_failure_reason = input.preview_failure_reason;
    artifacts.authority_trigger_layout = input.authority_trigger_layout;
    for (const auto& point : input.authority_trigger_points) {
        artifacts.authority_trigger_points.push_back(BuildWorkflowAuthorityTriggerPoint(point));
    }
    for (const auto& group : input.spacing_validation_groups) {
        artifacts.spacing_validation_groups.push_back(BuildWorkflowSpacingValidationGroup(group));
    }
    return artifacts;
}

AuthorityPreviewBuildResult BuildAuthorityPreviewBuildResult(const WorkflowAuthorityPreviewArtifacts& input) {
    AuthorityPreviewBuildResult authority_preview;
    authority_preview.segment_count = input.segment_count;
    authority_preview.total_length = input.total_length;
    authority_preview.estimated_time = input.estimated_time;
    authority_preview.preview_trajectory_points = input.preview_trajectory_points;
    authority_preview.glue_points = input.glue_points;
    authority_preview.trigger_count = input.trigger_count;
    authority_preview.dxf_filename = input.dxf_filename;
    authority_preview.timestamp = input.timestamp;
    authority_preview.preview_authority_ready = input.preview_authority_ready;
    authority_preview.preview_binding_ready = input.preview_binding_ready;
    authority_preview.preview_spacing_valid = input.preview_spacing_valid;
    authority_preview.preview_has_short_segment_exceptions = input.preview_has_short_segment_exceptions;
    authority_preview.preview_validation_classification = input.preview_validation_classification;
    authority_preview.preview_exception_reason = input.preview_exception_reason;
    authority_preview.preview_failure_reason = input.preview_failure_reason;
    authority_preview.authority_trigger_layout = input.authority_trigger_layout;
    for (const auto& point : input.authority_trigger_points) {
        authority_preview.authority_trigger_points.push_back(BuildInternalAuthorityTriggerPoint(point));
    }
    for (const auto& group : input.spacing_validation_groups) {
        authority_preview.spacing_validation_groups.push_back(BuildInternalSpacingValidationGroup(group));
    }
    return authority_preview;
}

ExecutionAssemblyBuildInput BuildExecutionAssemblyBuildInput(const WorkflowExecutionAssemblyRequest& input) {
    ExecutionAssemblyBuildInput execution_input;
    execution_input.process_path = input.process_path;
    execution_input.authority_process_path = input.authority_process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.dispensing_velocity = input.runtime_options.dispensing_velocity;
    execution_input.acceleration = input.runtime_options.acceleration;
    execution_input.dispenser_interval_ms = input.runtime_options.dispenser_interval_ms;
    execution_input.dispenser_duration_ms = input.runtime_options.dispenser_duration_ms;
    execution_input.trigger_spatial_interval_mm = input.runtime_options.trigger_spatial_interval_mm;
    execution_input.valve_response_ms = input.runtime_options.valve_response_ms;
    execution_input.safety_margin_ms = input.runtime_options.safety_margin_ms;
    execution_input.min_interval_ms = input.runtime_options.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.sample_dt = input.runtime_options.sample_dt;
    execution_input.sample_ds = input.runtime_options.sample_ds;
    execution_input.spline_max_step_mm = input.runtime_options.spline_max_step_mm;
    execution_input.spline_max_error_mm = input.runtime_options.spline_max_error_mm;
    execution_input.estimated_time_s = input.estimated_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.runtime_options.compensation_profile;
    execution_input.authority_preview = BuildAuthorityPreviewBuildResult(input.authority_preview);
    return execution_input;
}

WorkflowExecutionAssemblyResult BuildWorkflowExecutionAssemblyResult(const ExecutionAssemblyBuildResult& input) {
    WorkflowExecutionAssemblyResult result;
    result.execution_package = input.execution_package;
    result.execution_trajectory_points = input.execution_trajectory_points;
    result.motion_trajectory_points = input.motion_trajectory_points;
    result.preview_authority_shared_with_execution = input.preview_authority_shared_with_execution;
    result.execution_binding_ready = input.execution_binding_ready;
    result.execution_failure_reason = input.execution_failure_reason;
    result.authority_trigger_layout = input.authority_trigger_layout;
    result.export_request = input.export_request;
    return result;
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

const ProcessPath& ResolveAuthorityProcessPath(const PlanningArtifactsAssemblyInput& input) {
    if (!input.authority_process_path.segments.empty()) {
        return input.authority_process_path;
    }
    return input.process_path;
}

const ProcessPath& ResolveAuthorityProcessPath(const AuthorityPreviewBuildInput& input) {
    if (!input.authority_process_path.segments.empty()) {
        return input.authority_process_path;
    }
    return input.process_path;
}

const ProcessPath& ResolveExecutionProcessPath(const ExecutionAssemblyBuildInput& input) {
    if (!input.process_path.segments.empty()) {
        return input.process_path;
    }
    return input.authority_process_path;
}

float32 EstimateExecutionTime(
    const PlanningArtifactsAssemblyInput& input,
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

float32 ResolveInterpolationStep(const PlanningArtifactsAssemblyInput& input) {
    if (input.sample_ds > kEpsilon) {
        return input.sample_ds;
    }
    if (input.spline_max_step_mm > kEpsilon) {
        return input.spline_max_step_mm;
    }
    return 1.0f;
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

Result<std::vector<Point2D>> BuildInterpolationSeedPoints(
    const ProcessPath& path,
    float32 spline_max_error_mm,
    float32 step_mm) {
    std::vector<Point2D> points;
    Siligen::Domain::Dispensing::DomainServices::CurveFlatteningService flattening_service;
    const float32 sample_step_mm = step_mm > kEpsilon ? step_mm : 1.0f;
    const auto started_at = std::chrono::steady_clock::now();
    const std::size_t total_segments = path.segments.size();
    for (std::size_t segment_index = 0; segment_index < total_segments; ++segment_index) {
        const auto& segment = path.segments[segment_index];
        const auto& geometry = segment.geometry;
        if (geometry.is_point) {
            AppendUniquePoint(points, geometry.line.start);
        } else {
            auto flattened_result = flattening_service.Flatten(geometry, spline_max_error_mm, step_mm);
            if (flattened_result.IsError()) {
                return Result<std::vector<Point2D>>::Failure(flattened_result.GetError());
            }
            const auto& flattened_points = flattened_result.Value().points;
            if (!flattened_points.empty()) {
                AppendUniquePoint(points, flattened_points.front());
                for (std::size_t index = 1; index < flattened_points.size(); ++index) {
                    SampleLinePoints(flattened_points[index - 1], flattened_points[index], sample_step_mm, points);
                }
            }
        }

        if (total_segments >= 200 &&
            (((segment_index + 1) % 100U) == 0U || (segment_index + 1) == total_segments)) {
            std::ostringstream oss;
            oss << "build_interp_seed_stage=progress"
                << " processed_segments=" << (segment_index + 1)
                << " total_segments=" << total_segments
                << " sampled_points=" << points.size()
                << " elapsed_ms=" << ElapsedMs(started_at);
            SILIGEN_LOG_INFO(oss.str());
        }
    }
    return Result<std::vector<Point2D>>::Success(std::move(points));
}

std::vector<TrajectoryPoint> ConvertPreviewPointsToTrajectory(
    const std::vector<Point2D>& preview_points) {
    std::vector<TrajectoryPoint> points;
    points.reserve(preview_points.size());
    for (std::size_t index = 0; index < preview_points.size(); ++index) {
        TrajectoryPoint point;
        point.position = preview_points[index];
        point.sequence_id = static_cast<uint32>(index);
        points.push_back(point);
    }
    return points;
}

float32 EstimatePreviewTime(
    const AuthorityPreviewBuildInput& input,
    float32 total_length_mm) {
    if (input.dispensing_velocity <= kEpsilon || total_length_mm <= kEpsilon) {
        return 0.0f;
    }
    return total_length_mm / input.dispensing_velocity;
}

std::vector<Point2D> CollectAuthorityPositions(const AuthorityTriggerLayout& layout) {
    std::vector<Point2D> positions;
    positions.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        positions.push_back(trigger.position);
    }
    return positions;
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
