#pragma once

#include "../../services/lifecycle/HandleStageFailedService.h"
#include "../../services/lifecycle/HandleStageSucceededService.h"

#include <utility>

namespace Siligen::Workflow::Runtime::Orchestrators {

class WorkflowEventOrchestrator {
   public:
    [[nodiscard]] Domain::WorkflowRun OnStageSucceeded(
        Domain::WorkflowRun workflow_run,
        const Contracts::StageSucceeded& event) const {
        return success_service_.Apply(std::move(workflow_run), event);
    }

    [[nodiscard]] Domain::WorkflowRun OnStageFailed(
        Domain::WorkflowRun workflow_run,
        const Contracts::StageFailed& event) const {
        return failure_service_.Apply(std::move(workflow_run), event);
    }

   private:
    Services::Lifecycle::HandleStageSucceededService success_service_{};
    Services::Lifecycle::HandleStageFailedService failure_service_{};
};

}  // namespace Siligen::Workflow::Runtime::Orchestrators
