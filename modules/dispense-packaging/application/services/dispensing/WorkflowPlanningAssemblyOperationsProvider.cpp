#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"

#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"
#include "application/services/dispensing/PlanningAssemblyTypes.h"

#include <memory>
#include <utility>

namespace Siligen::Application::Services::Dispensing {
namespace {

using Siligen::Shared::Types::Result;

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

WorkflowAssemblyRuntimeOptions BuildRuntimeOptions(const AuthorityPreviewBuildInput& input) {
    WorkflowAssemblyRuntimeOptions options;
    options.dispensing_velocity = input.dispensing_velocity;
    options.acceleration = input.acceleration;
    options.dispenser_interval_ms = input.dispenser_interval_ms;
    options.dispenser_duration_ms = input.dispenser_duration_ms;
    options.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    options.valve_response_ms = input.valve_response_ms;
    options.safety_margin_ms = input.safety_margin_ms;
    options.min_interval_ms = input.min_interval_ms;
    options.sample_dt = input.sample_dt;
    options.sample_ds = input.sample_ds;
    options.spline_max_step_mm = input.spline_max_step_mm;
    options.spline_max_error_mm = input.spline_max_error_mm;
    options.compensation_profile = input.compensation_profile;
    return options;
}

WorkflowAssemblyRuntimeOptions BuildRuntimeOptions(const ExecutionAssemblyBuildInput& input) {
    WorkflowAssemblyRuntimeOptions options;
    options.dispensing_velocity = input.dispensing_velocity;
    options.acceleration = input.acceleration;
    options.dispenser_interval_ms = input.dispenser_interval_ms;
    options.dispenser_duration_ms = input.dispenser_duration_ms;
    options.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    options.valve_response_ms = input.valve_response_ms;
    options.safety_margin_ms = input.safety_margin_ms;
    options.min_interval_ms = input.min_interval_ms;
    options.sample_dt = input.sample_dt;
    options.sample_ds = input.sample_ds;
    options.spline_max_step_mm = input.spline_max_step_mm;
    options.spline_max_error_mm = input.spline_max_error_mm;
    options.compensation_profile = input.compensation_profile;
    return options;
}

AuthorityPreviewBuildInput BuildAuthorityPreviewInput(const WorkflowAuthorityPreviewRequest& input) {
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

AuthorityPreviewBuildResult BuildAuthorityPreviewResult(const WorkflowAuthorityPreviewArtifacts& input) {
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

ExecutionAssemblyBuildInput BuildExecutionAssemblyInput(const WorkflowExecutionAssemblyRequest& input) {
    ExecutionAssemblyBuildInput execution_input;
    execution_input.process_path = input.process_path;
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
    execution_input.authority_preview = BuildAuthorityPreviewResult(input.authority_preview);
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

WorkflowExecutionAssemblyRequest BuildWorkflowExecutionAssemblyRequest(
    const WorkflowPlanningAssemblyRequest& input,
    const WorkflowAuthorityPreviewArtifacts& authority_preview) {
    WorkflowExecutionAssemblyRequest request;
    request.process_path = input.authority_preview_request.process_path;
    request.motion_plan = input.motion_plan;
    request.source_path = input.authority_preview_request.source_path;
    request.dxf_filename = input.authority_preview_request.dxf_filename;
    request.runtime_options = input.authority_preview_request.runtime_options;
    request.max_jerk = input.max_jerk;
    request.estimated_time_s = input.estimated_time_s;
    request.use_interpolation_planner = input.use_interpolation_planner;
    request.interpolation_algorithm = input.interpolation_algorithm;
    request.authority_preview = authority_preview;
    return request;
}

class WorkflowPlanningAssemblyOperations final : public IWorkflowPlanningAssemblyOperations {
public:
    WorkflowPlanningAssemblyOperations(
        AuthorityPreviewAssemblyService authority_preview_service,
        ExecutionAssemblyService execution_assembly_service)
        : authority_preview_service_(std::move(authority_preview_service)),
          execution_assembly_service_(std::move(execution_assembly_service)) {}

    Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifacts(
        const WorkflowAuthorityPreviewRequest& input) const override {
        auto result = authority_preview_service_.BuildAuthorityPreviewArtifacts(BuildAuthorityPreviewInput(input));
        if (result.IsError()) {
            return Result<WorkflowAuthorityPreviewArtifacts>::Failure(result.GetError());
        }
        return Result<WorkflowAuthorityPreviewArtifacts>::Success(
            BuildWorkflowAuthorityPreviewArtifacts(result.Value()));
    }

    Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthority(
        const WorkflowExecutionAssemblyRequest& input) const override {
        auto result = execution_assembly_service_.BuildExecutionArtifactsFromAuthority(
            BuildExecutionAssemblyInput(input));
        if (result.IsError()) {
            return Result<WorkflowExecutionAssemblyResult>::Failure(result.GetError());
        }
        return Result<WorkflowExecutionAssemblyResult>::Success(
            BuildWorkflowExecutionAssemblyResult(result.Value()));
    }

    Result<WorkflowPlanningAssemblyResult> AssemblePlanningArtifacts(
        const WorkflowPlanningAssemblyRequest& input) const override {
        auto authority_result = BuildAuthorityPreviewArtifacts(input.authority_preview_request);
        if (authority_result.IsError()) {
            return Result<WorkflowPlanningAssemblyResult>::Failure(authority_result.GetError());
        }

        auto execution_result = BuildExecutionArtifactsFromAuthority(
            BuildWorkflowExecutionAssemblyRequest(input, authority_result.Value()));
        if (execution_result.IsError()) {
            return Result<WorkflowPlanningAssemblyResult>::Failure(execution_result.GetError());
        }

        const auto& authority_preview = authority_result.Value();
        const auto& execution = execution_result.Value();

        WorkflowPlanningAssemblyResult result;
        result.execution_package = execution.execution_package;
        result.segment_count = authority_preview.segment_count;
        result.total_length = execution.execution_package.total_length_mm;
        result.estimated_time = execution.execution_package.estimated_time_s;
        result.trajectory_points = execution.execution_trajectory_points;
        result.glue_points = authority_preview.glue_points;
        result.trigger_count = authority_preview.trigger_count;
        result.dxf_filename = authority_preview.dxf_filename;
        result.timestamp = authority_preview.timestamp;
        result.planning_report = input.motion_plan.planning_report;
        result.preview_authority_ready = authority_preview.preview_authority_ready;
        result.preview_authority_shared_with_execution = execution.preview_authority_shared_with_execution;
        result.preview_binding_ready = execution.execution_binding_ready;
        result.preview_spacing_valid = authority_preview.preview_spacing_valid;
        result.preview_has_short_segment_exceptions = authority_preview.preview_has_short_segment_exceptions;
        result.preview_validation_classification = authority_preview.preview_validation_classification;
        result.preview_exception_reason = authority_preview.preview_exception_reason;
        result.preview_failure_reason =
            execution.execution_failure_reason.empty()
                ? authority_preview.preview_failure_reason
                : execution.execution_failure_reason;
        result.authority_trigger_layout = execution.authority_trigger_layout;
        result.authority_trigger_points = authority_preview.authority_trigger_points;
        result.spacing_validation_groups = authority_preview.spacing_validation_groups;
        result.export_request = execution.export_request;
        return Result<WorkflowPlanningAssemblyResult>::Success(std::move(result));
    }

private:
    AuthorityPreviewAssemblyService authority_preview_service_;
    ExecutionAssemblyService execution_assembly_service_;
};

}  // namespace

std::shared_ptr<IWorkflowPlanningAssemblyOperations>
WorkflowPlanningAssemblyOperationsProvider::CreateOperations() const {
    return std::make_shared<WorkflowPlanningAssemblyOperations>(
        AuthorityPreviewAssemblyService{},
        ExecutionAssemblyService{});
}

}  // namespace Siligen::Application::Services::Dispensing
