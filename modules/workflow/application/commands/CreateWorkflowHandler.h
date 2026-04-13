#pragma once

#include "../../services/lifecycle/CreateWorkflowService.h"

namespace Siligen::Workflow::Application::Commands {

class CreateWorkflowHandler {
   public:
    [[nodiscard]] Domain::WorkflowRun Handle(const Contracts::CreateWorkflow& command) const {
        return service_.Create(command);
    }

   private:
    Services::Lifecycle::CreateWorkflowService service_{};
};

}  // namespace Siligen::Workflow::Application::Commands
