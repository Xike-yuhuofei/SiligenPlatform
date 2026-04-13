#pragma once

#include "../orchestrators/WorkflowArchiveOrchestrator.h"

#include <utility>

namespace Siligen::Workflow::Runtime::Subscriptions {

class TraceEventsSubscription {
   public:
    [[nodiscard]] Domain::WorkflowRun OnArchiveCompleted(
        Domain::WorkflowRun workflow_run,
        std::int64_t occurred_at) const {
        return archive_orchestrator_.CompleteArchive(std::move(workflow_run), occurred_at);
    }

   private:
    Orchestrators::WorkflowArchiveOrchestrator archive_orchestrator_{};
};

}  // namespace Siligen::Workflow::Runtime::Subscriptions
