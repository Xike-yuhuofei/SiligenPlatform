#pragma once

#include "application/services/dispensing/IPlanningArtifactExportPort.h"
#include "../../usecases/dispensing/PlanningUseCase.h"

namespace Siligen::Application::Services::Dispensing {

struct PlanningPreviewAssemblyResult {
    UseCases::Dispensing::PlanningResponse response;
    PlanningArtifactExportRequest export_request;
};

class PlanningPreviewAssemblyService {
public:
    Shared::Types::Result<PlanningPreviewAssemblyResult> BuildResponse(
        const UseCases::Dispensing::PlanningRequest& request,
        const Domain::Dispensing::DomainServices::DispensingPlan& plan,
        const Domain::Dispensing::DomainServices::DispensingPlanRequest& plan_request,
        const std::string& dxf_filename) const;
};

}  // namespace Siligen::Application::Services::Dispensing
