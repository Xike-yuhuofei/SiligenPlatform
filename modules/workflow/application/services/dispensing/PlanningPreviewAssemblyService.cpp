#include "PlanningPreviewAssemblyService.h"

#include "application/services/dispensing/DispensePlanningFacade.h"

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Shared::Types::Result;

Result<PlanningPreviewAssemblyResult> PlanningPreviewAssemblyService::BuildResponse(
    const PlanningRequest& request,
    const DispensingPlan& plan,
    const DispensingPlanRequest& plan_request,
    const std::string& dxf_filename) const {
    DispensePlanningFacade facade;

    PlanningPreviewAssemblyInput input;
    input.plan = plan;
    input.plan_request = plan_request;
    input.source_path = request.dxf_filepath;
    input.dxf_filename = dxf_filename;
    input.spacing_tol_ratio = request.spacing_tol_ratio;
    input.spacing_min_mm = request.spacing_min_mm;
    input.spacing_max_mm = request.spacing_max_mm;

    auto payload_result = facade.BuildPreviewPayload(input);
    if (payload_result.IsError()) {
        return Result<PlanningPreviewAssemblyResult>::Failure(payload_result.GetError());
    }

    const auto& payload = payload_result.Value();

    PlanningPreviewAssemblyResult result;
    result.response.success = true;
    result.response.segment_count = payload.segment_count;
    result.response.total_length = payload.total_length;
    result.response.estimated_time = payload.estimated_time;
    result.response.execution_trajectory_points = payload.execution_trajectory_points;
    result.response.glue_points = payload.glue_points;
    result.response.process_tags.assign(result.response.execution_trajectory_points.size(), 0);
    result.response.trigger_count = payload.trigger_count;
    result.response.dxf_filename = payload.dxf_filename;
    result.response.timestamp = payload.timestamp;
    result.response.planning_report = payload.planning_report;
    result.export_request = payload.export_request;
    return Result<PlanningPreviewAssemblyResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing
