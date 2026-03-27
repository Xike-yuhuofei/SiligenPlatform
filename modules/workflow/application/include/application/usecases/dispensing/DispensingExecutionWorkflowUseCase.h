#pragma once

#include "application/usecases/dispensing/PlanningUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing {

class DispensingExecutionWorkflowUseCase {
   public:
    DispensingExecutionWorkflowUseCase(
        std::shared_ptr<PlanningUseCase> planning_use_case,
        std::shared_ptr<DispensingExecutionUseCase> execution_use_case);

    ~DispensingExecutionWorkflowUseCase() = default;

    DispensingExecutionWorkflowUseCase(const DispensingExecutionWorkflowUseCase&) = delete;
    DispensingExecutionWorkflowUseCase& operator=(const DispensingExecutionWorkflowUseCase&) = delete;

    Result<DispensingMVPResult> Execute(const DispensingMVPRequest& request);
    Result<TaskID> ExecuteAsync(const DispensingMVPRequest& request);

   private:
    std::shared_ptr<PlanningUseCase> planning_use_case_;
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case_;
};

}  // namespace Siligen::Application::UseCases::Dispensing
