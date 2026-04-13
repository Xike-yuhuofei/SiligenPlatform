#pragma once

#include "../../services/rollback/RequestRollbackService.h"
#include "../../services/rollback/RollbackImpactAnalysisService.h"

namespace Siligen::Workflow::Application::Commands {

class RequestRollbackHandler {
   public:
    [[nodiscard]] Domain::RollbackDecision Handle(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RequestRollback& command,
        std::int64_t occurred_at) const {
        auto decision = request_service_.CreateDecision(workflow_run, command, occurred_at);
        impact_service_.Apply(decision, workflow_run);
        return decision;
    }

   private:
    Services::Rollback::RequestRollbackService request_service_{};
    Services::Rollback::RollbackImpactAnalysisService impact_service_{};
};

}  // namespace Siligen::Workflow::Application::Commands
