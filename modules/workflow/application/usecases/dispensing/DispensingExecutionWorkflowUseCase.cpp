#define MODULE_NAME "DispensingExecutionWorkflowUseCase"

#include "DispensingExecutionWorkflowUseCase.h"

#include "application/services/dispensing/DispensingExecutionCompatibilityService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <stdexcept>
#include <utility>

namespace Siligen::Application::UseCases::Dispensing {

DispensingExecutionWorkflowUseCase::DispensingExecutionWorkflowUseCase(
    std::shared_ptr<PlanningUseCase> planning_use_case,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case)
    : planning_use_case_(std::move(planning_use_case)),
      execution_use_case_(std::move(execution_use_case)) {
    if (!planning_use_case_ || !execution_use_case_) {
        throw std::invalid_argument("DispensingExecutionWorkflowUseCase: dependencies cannot be null");
    }
}

Result<DispensingMVPResult> DispensingExecutionWorkflowUseCase::Execute(const DispensingMVPRequest& request) {
    Services::Dispensing::DispensingExecutionCompatibilityService compatibility_service;

    auto planning_request_result = compatibility_service.BuildPlanningRequest(request);
    if (planning_request_result.IsError()) {
        return Result<DispensingMVPResult>::Failure(planning_request_result.GetError());
    }

    auto planning_result = planning_use_case_->Execute(planning_request_result.Value());
    if (planning_result.IsError()) {
        return Result<DispensingMVPResult>::Failure(planning_result.GetError());
    }

    auto execution_request_result = compatibility_service.BuildExecutionRequest(planning_result.Value(), request);
    if (execution_request_result.IsError()) {
        return Result<DispensingMVPResult>::Failure(execution_request_result.GetError());
    }

    return execution_use_case_->Execute(execution_request_result.Value());
}

Result<TaskID> DispensingExecutionWorkflowUseCase::ExecuteAsync(const DispensingMVPRequest& request) {
    Services::Dispensing::DispensingExecutionCompatibilityService compatibility_service;

    auto planning_request_result = compatibility_service.BuildPlanningRequest(request);
    if (planning_request_result.IsError()) {
        return Result<TaskID>::Failure(planning_request_result.GetError());
    }

    auto planning_result = planning_use_case_->Execute(planning_request_result.Value());
    if (planning_result.IsError()) {
        return Result<TaskID>::Failure(planning_result.GetError());
    }

    auto execution_request_result = compatibility_service.BuildExecutionRequest(planning_result.Value(), request);
    if (execution_request_result.IsError()) {
        return Result<TaskID>::Failure(execution_request_result.GetError());
    }

    return execution_use_case_->ExecuteAsync(execution_request_result.Value());
}

}  // namespace Siligen::Application::UseCases::Dispensing
