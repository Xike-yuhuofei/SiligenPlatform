#pragma once

#include "../../services/archive/RequestWorkflowArchiveService.h"

#include <utility>

namespace Siligen::Workflow::Application::Commands {

class CompleteWorkflowArchiveHandler {
   public:
    [[nodiscard]] Domain::WorkflowRun Handle(
        Domain::WorkflowRun workflow_run,
        const Contracts::CompleteWorkflowArchive& command,
        std::int64_t occurred_at) const {
        return service_.Apply(std::move(workflow_run), command, occurred_at);
    }

   private:
    Services::Archive::RequestWorkflowArchiveService service_{};
};

}  // namespace Siligen::Workflow::Application::Commands
