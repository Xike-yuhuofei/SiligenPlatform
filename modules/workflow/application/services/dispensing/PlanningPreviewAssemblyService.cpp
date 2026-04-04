#include "PlanningPreviewAssemblyService.h"

#include "shared/types/Error.h"

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<PlanningPreviewAssemblyResult> PlanningPreviewAssemblyService::BuildResponse(
    const PlanningRequest& request,
    const DispensingPlan& plan,
    const DispensingPlanRequest& plan_request,
    const std::string& dxf_filename) const {
    (void)request;
    (void)plan;
    (void)plan_request;
    (void)dxf_filename;

    // Retired compatibility shim: workflow must consume the canonical
    // authority-preview + execution assembly chain instead of rebuilding M8 truth locally.
    return Result<PlanningPreviewAssemblyResult>::Failure(
        Error(
            ErrorCode::INVALID_STATE,
            "PlanningPreviewAssemblyService 已退役；请改用 PlanningUseCase authority/execution chain",
            "PlanningPreviewAssemblyService"));
}

}  // namespace Siligen::Application::Services::Dispensing
