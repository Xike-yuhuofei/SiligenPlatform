#pragma once

#include "workflow/contracts/commands/CompleteWorkflowArchive.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Archive {

class RequestWorkflowArchiveService {
   public:
    [[nodiscard]] Domain::WorkflowRun Apply(
        Domain::WorkflowRun workflow_run,
        const Contracts::CompleteWorkflowArchive& command,
        std::int64_t occurred_at) const {
        if (workflow_run.CanRequestArchive()) {
            workflow_run.MarkArchiveRequested(occurred_at);
            workflow_run.pending_request_id = command.request_id;
        }
        return workflow_run;
    }
};

}  // namespace Siligen::Workflow::Services::Archive
