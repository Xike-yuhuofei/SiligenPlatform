#pragma once

#include "../../services/lifecycle/AdvanceStageService.h"

namespace Siligen::Workflow::Application::Commands {

class AdvanceStageHandler {
   public:
    [[nodiscard]] Domain::StageTransitionRecord Handle(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::AdvanceStage& command,
        std::int64_t occurred_at) const {
        return service_.PrepareTransition(workflow_run, command, occurred_at);
    }

   private:
    Services::Lifecycle::AdvanceStageService service_{};
};

}  // namespace Siligen::Workflow::Application::Commands
