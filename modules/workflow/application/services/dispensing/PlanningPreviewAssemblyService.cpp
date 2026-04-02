#include "PlanningPreviewAssemblyService.h"

#include "application/services/dispensing/DispensePlanningFacade.h"

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Shared::Types::Result;

Result<PlanningPreviewAssemblyResult> PlanningPreviewAssemblyService::BuildResponse(
    const PlanningRequest& request,
    const DispensingPlan& plan,
    const DispensingPlanRequest& plan_request,
    const std::string& dxf_filename) const {
    DispensePlanningFacade facade;

    PlanningArtifactsBuildInput input;
    input.process_path = plan.process_path;
    input.authority_process_path = plan.process_path;
    input.motion_plan = plan.motion_trajectory;
    input.source_path = request.dxf_filepath;
    input.dxf_filename = dxf_filename;
    input.dispensing_velocity = plan_request.dispensing_velocity;
    input.acceleration = plan_request.acceleration;
    input.dispenser_interval_ms = plan_request.dispenser_interval_ms;
    input.dispenser_duration_ms = plan_request.dispenser_duration_ms;
    input.trigger_spatial_interval_mm = plan_request.trigger_spatial_interval_mm;
    input.valve_response_ms = plan_request.valve_response_ms;
    input.safety_margin_ms = plan_request.safety_margin_ms;
    input.min_interval_ms = plan_request.min_interval_ms;
    input.max_jerk = plan_request.max_jerk;
    input.sample_dt = plan_request.sample_dt;
    input.sample_ds = plan_request.sample_ds;
    input.spline_max_step_mm = plan_request.spline_max_step_mm;
    input.spline_max_error_mm = plan_request.spline_max_error_mm;
    input.estimated_time_s = plan.estimated_time_s;
    input.dispensing_strategy = plan_request.dispensing_strategy;
    input.subsegment_count = plan_request.subsegment_count;
    input.dispense_only_cruise = plan_request.dispense_only_cruise;
    input.downgrade_on_violation = plan_request.downgrade_on_violation;
    input.use_interpolation_planner = plan_request.use_interpolation_planner;
    input.interpolation_algorithm = plan_request.interpolation_algorithm;
    input.compensation_profile = plan_request.compensation_profile;
    input.spacing_tol_ratio = request.spacing_tol_ratio;
    input.spacing_min_mm = request.spacing_min_mm;
    input.spacing_max_mm = request.spacing_max_mm;

    auto assembly_result = facade.AssemblePlanningArtifacts(input);
    if (assembly_result.IsError()) {
        return Result<PlanningPreviewAssemblyResult>::Failure(assembly_result.GetError());
    }

    const auto& assembled = assembly_result.Value();

    PlanningPreviewAssemblyResult result;
    result.response.success = true;
    result.response.segment_count = assembled.segment_count;
    result.response.total_length = assembled.total_length;
    result.response.estimated_time = assembled.estimated_time;
    result.response.execution_trajectory_points = assembled.trajectory_points;
    result.response.glue_points = assembled.glue_points;
    result.response.process_tags.assign(result.response.execution_trajectory_points.size(), 0);
    result.response.trigger_count = assembled.trigger_count;
    result.response.dxf_filename = assembled.dxf_filename;
    result.response.timestamp = assembled.timestamp;
    result.response.planning_report = assembled.planning_report;
    result.response.preview_authority_ready = assembled.preview_authority_ready;
    result.response.preview_authority_shared_with_execution = assembled.preview_authority_shared_with_execution;
    result.response.preview_binding_ready = assembled.preview_binding_ready;
    result.response.preview_spacing_valid = assembled.preview_spacing_valid;
    result.response.preview_has_short_segment_exceptions = assembled.preview_has_short_segment_exceptions;
    result.response.preview_validation_classification = assembled.preview_validation_classification;
    result.response.preview_exception_reason = assembled.preview_exception_reason;
    result.response.preview_failure_reason = assembled.preview_failure_reason;
    result.response.authority_trigger_layout = assembled.authority_trigger_layout;
    result.response.authority_trigger_points = assembled.authority_trigger_points;
    result.response.spacing_validation_groups = assembled.spacing_validation_groups;
    result.export_request = assembled.export_request;
    return Result<PlanningPreviewAssemblyResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing
