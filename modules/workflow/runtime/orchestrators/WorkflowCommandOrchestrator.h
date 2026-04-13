#pragma once

#include "../../application/facade/WorkflowFacade.h"

namespace Siligen::Workflow::Runtime::Orchestrators {

class WorkflowCommandOrchestrator {
   public:
    [[nodiscard]] Domain::WorkflowRun CreateWorkflow(const Contracts::CreateWorkflow& command) const {
        return facade_.CreateWorkflow(command);
    }

    [[nodiscard]] Domain::StageTransitionRecord AdvanceStage(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::AdvanceStage& command,
        std::int64_t occurred_at) const {
        return facade_.AdvanceStage(workflow_run, command, occurred_at);
    }

    [[nodiscard]] Domain::RollbackDecision RequestRollback(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RequestRollback& command,
        std::int64_t occurred_at) const {
        return facade_.RequestRollback(workflow_run, command, occurred_at);
    }

   private:
    Application::Facade::WorkflowFacade facade_{};
};

}  // namespace Siligen::Workflow::Runtime::Orchestrators
