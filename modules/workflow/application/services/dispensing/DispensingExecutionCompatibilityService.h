#pragma once

#include "../../usecases/dispensing/PlanningUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <string>

namespace Siligen::Application::Services::Dispensing {

class DispensingExecutionCompatibilityService {
   public:
    Shared::Types::Result<UseCases::Dispensing::PlanningRequest> BuildPlanningRequest(
        const UseCases::Dispensing::DispensingMVPRequest& request,
        const std::string& source_path = std::string()) const;

    Shared::Types::Result<UseCases::Dispensing::DispensingExecutionRequest> BuildExecutionRequest(
        const UseCases::Dispensing::PlanningResponse& planning_response,
        const UseCases::Dispensing::DispensingMVPRequest& request,
        const std::string& source_path = std::string()) const;
};

}  // namespace Siligen::Application::Services::Dispensing
