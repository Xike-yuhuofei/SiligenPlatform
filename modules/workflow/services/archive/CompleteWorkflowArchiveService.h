#pragma once

#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Archive {

class CompleteWorkflowArchiveService {
   public:
    [[nodiscard]] Domain::WorkflowRun Apply(
        Domain::WorkflowRun workflow_run,
        std::int64_t occurred_at) const {
        workflow_run.MarkArchived(occurred_at);
        return workflow_run;
    }
};

}  // namespace Siligen::Workflow::Services::Archive
