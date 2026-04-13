#pragma once

#include "../../services/archive/CompleteWorkflowArchiveService.h"
#include "../../services/archive/RequestWorkflowArchiveService.h"

#include <utility>

namespace Siligen::Workflow::Runtime::Orchestrators {

class WorkflowArchiveOrchestrator {
   public:
    [[nodiscard]] Domain::WorkflowRun RequestArchive(
        Domain::WorkflowRun workflow_run,
        const Contracts::CompleteWorkflowArchive& command,
        std::int64_t occurred_at) const {
        return request_service_.Apply(std::move(workflow_run), command, occurred_at);
    }

    [[nodiscard]] Domain::WorkflowRun CompleteArchive(
        Domain::WorkflowRun workflow_run,
        std::int64_t occurred_at) const {
        return complete_service_.Apply(std::move(workflow_run), occurred_at);
    }

   private:
    Services::Archive::RequestWorkflowArchiveService request_service_{};
    Services::Archive::CompleteWorkflowArchiveService complete_service_{};
};

}  // namespace Siligen::Workflow::Runtime::Orchestrators
