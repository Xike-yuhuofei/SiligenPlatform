#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"

#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"

#include <memory>
#include <utility>

namespace Siligen::Application::Services::Dispensing {
namespace {

using Siligen::Shared::Types::Result;

WorkflowExecutionAssemblyRequest BuildWorkflowExecutionAssemblyRequest(
    const WorkflowPlanningAssemblyRequest& input,
    const WorkflowAuthorityPreviewArtifacts& authority_preview) {
    WorkflowExecutionAssemblyRequest request;
    request.process_path = input.authority_preview_request.process_path;
    request.authority_process_path = input.authority_preview_request.authority_process_path;
    request.motion_plan = input.motion_plan;
    request.planning_start_position = input.planning_start_position;
    request.recipe_id = input.recipe_id;
    request.version_id = input.version_id;
    request.source_path = input.authority_preview_request.source_path;
    request.dxf_filename = input.authority_preview_request.dxf_filename;
    request.runtime_options = input.authority_preview_request.runtime_options;
    request.max_jerk = input.max_jerk;
    request.estimated_time_s = input.estimated_time_s;
    request.use_interpolation_planner = input.use_interpolation_planner;
    request.interpolation_algorithm = input.interpolation_algorithm;
    request.requested_execution_strategy = input.requested_execution_strategy;
    request.point_flying_carrier_policy = input.point_flying_carrier_policy;
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
        return authority_preview_service_.BuildAuthorityPreviewArtifacts(input);
    }

    Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthority(
        const WorkflowExecutionAssemblyRequest& input) const override {
        return execution_assembly_service_.BuildExecutionArtifactsFromAuthority(input);
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
