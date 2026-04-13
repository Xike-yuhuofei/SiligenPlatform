#pragma once

#include "../../services/lifecycle/RetryStageService.h"

namespace Siligen::Workflow::Application::Commands {

class RetryStageHandler {
   public:
    [[nodiscard]] Domain::StageTransitionRecord Handle(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RetryStage& command,
        std::int64_t occurred_at) const {
        return service_.PrepareRetryTransition(workflow_run, command, occurred_at);
    }

   private:
    Services::Lifecycle::RetryStageService service_{};
};

}  // namespace Siligen::Workflow::Application::Commands
