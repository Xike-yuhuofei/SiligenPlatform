#pragma once

#include "workflow/contracts/commands/AbortWorkflow.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Application::Commands {

class AbortWorkflowHandler {
   public:
    [[nodiscard]] Domain::WorkflowRun Handle(
        Domain::WorkflowRun workflow_run,
        const Contracts::AbortWorkflow& command,
        std::int64_t occurred_at) const {
        workflow_run.ApplyState(Domain::WorkflowRunState::Aborted, "Aborted", occurred_at);
        workflow_run.pending_request_id = command.request_id;
        workflow_run.pending_command_id.clear();
        workflow_run.aborted_at = occurred_at;
        return workflow_run;
    }
};

}  // namespace Siligen::Workflow::Application::Commands
