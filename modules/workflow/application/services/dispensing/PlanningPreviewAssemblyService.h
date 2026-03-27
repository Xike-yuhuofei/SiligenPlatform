#pragma once

#include "../../usecases/dispensing/PlanningUseCase.h"

#include <ctime>
#include <string>

namespace Siligen::Application::Services::Dispensing {

class PlanningPreviewAssemblyService {
public:
    Shared::Types::Result<UseCases::Dispensing::PlanningResponse> BuildResponse(
        const UseCases::Dispensing::PlanningRequest& request,
        const Domain::Dispensing::DomainServices::DispensingPlan& plan,
        const Domain::Dispensing::DomainServices::DispensingPlanRequest& plan_request,
        bool dump_preview,
        std::time_t dump_timestamp,
        const std::string& dxf_filename) const;
};

}  // namespace Siligen::Application::Services::Dispensing
