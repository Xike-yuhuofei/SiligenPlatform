#pragma once

#include "../orchestrators/WorkflowEventOrchestrator.h"

#include <utility>

namespace Siligen::Workflow::Runtime::Subscriptions {

class PlanningEventsSubscription {
   public:
    [[nodiscard]] Domain::WorkflowRun OnPlanningSucceeded(
        Domain::WorkflowRun workflow_run,
        const Contracts::StageSucceeded& event) const {
        return orchestrator_.OnStageSucceeded(std::move(workflow_run), event);
    }

    [[nodiscard]] Domain::WorkflowRun OnPlanningFailed(
        Domain::WorkflowRun workflow_run,
        const Contracts::StageFailed& event) const {
        return orchestrator_.OnStageFailed(std::move(workflow_run), event);
    }

   private:
    Orchestrators::WorkflowEventOrchestrator orchestrator_{};
};

}  // namespace Siligen::Workflow::Runtime::Subscriptions
