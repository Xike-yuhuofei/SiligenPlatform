#pragma once

#include "workflow/contracts/commands/CreateWorkflow.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Lifecycle {

class CreateWorkflowService {
   public:
    [[nodiscard]] Domain::WorkflowRun Create(const Contracts::CreateWorkflow& command) const {
        Domain::WorkflowRun workflow_run;
        workflow_run.workflow_id = command.workflow_id;
        workflow_run.job_id = command.job_id;
        workflow_run.current_stage = command.initial_stage.empty() ? "Created" : command.initial_stage;
        workflow_run.target_stage = workflow_run.current_stage;
        workflow_run.pending_request_id = command.request_id;
        workflow_run.current_execution_context = command.context_snapshot_ref;
        workflow_run.created_at = command.issued_at;
        workflow_run.updated_at = command.issued_at;
        return workflow_run;
    }
};

}  // namespace Siligen::Workflow::Services::Lifecycle
