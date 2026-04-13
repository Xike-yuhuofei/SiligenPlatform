#pragma once

#include "../commands/AbortWorkflowHandler.h"
#include "../commands/AdvanceStageHandler.h"
#include "../commands/CompleteWorkflowArchiveHandler.h"
#include "../commands/CreateWorkflowHandler.h"
#include "../commands/PerformRollbackHandler.h"
#include "../commands/RequestRollbackHandler.h"
#include "../commands/RetryStageHandler.h"
#include "../queries/GetRollbackDecisionHandler.h"
#include "../queries/GetWorkflowRunHandler.h"
#include "../queries/GetWorkflowStageHistoryHandler.h"
#include "../queries/GetWorkflowTimelineHandler.h"

#include <utility>
#include <vector>

namespace Siligen::Workflow::Application::Facade {

class WorkflowFacade {
   public:
    [[nodiscard]] Domain::WorkflowRun CreateWorkflow(const Contracts::CreateWorkflow& command) const {
        return create_handler_.Handle(command);
    }

    [[nodiscard]] Domain::StageTransitionRecord AdvanceStage(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::AdvanceStage& command,
        std::int64_t occurred_at) const {
        return advance_handler_.Handle(workflow_run, command, occurred_at);
    }

    [[nodiscard]] Domain::StageTransitionRecord RetryStage(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RetryStage& command,
        std::int64_t occurred_at) const {
        return retry_handler_.Handle(workflow_run, command, occurred_at);
    }

    [[nodiscard]] Domain::RollbackDecision RequestRollback(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RequestRollback& command,
        std::int64_t occurred_at) const {
        return request_rollback_handler_.Handle(workflow_run, command, occurred_at);
    }

    [[nodiscard]] Domain::WorkflowRun PerformRollback(
        Domain::WorkflowRun workflow_run,
        const Domain::RollbackDecision& decision,
        const Contracts::PerformRollback& command,
        std::int64_t occurred_at) const {
        return perform_rollback_handler_.Handle(std::move(workflow_run), decision, command, occurred_at);
    }

    [[nodiscard]] Domain::WorkflowRun AbortWorkflow(
        Domain::WorkflowRun workflow_run,
        const Contracts::AbortWorkflow& command,
        std::int64_t occurred_at) const {
        return abort_handler_.Handle(std::move(workflow_run), command, occurred_at);
    }

    [[nodiscard]] Domain::WorkflowRun RequestArchive(
        Domain::WorkflowRun workflow_run,
        const Contracts::CompleteWorkflowArchive& command,
        std::int64_t occurred_at) const {
        return archive_handler_.Handle(std::move(workflow_run), command, occurred_at);
    }

    [[nodiscard]] Contracts::WorkflowRunView GetWorkflowRun(const Domain::WorkflowRun& workflow_run) const {
        return workflow_run_query_.Handle(workflow_run);
    }

    [[nodiscard]] std::vector<Contracts::StageTransitionRecordView> GetWorkflowStageHistory(
        const std::vector<Domain::StageTransitionRecord>& transitions) const {
        return stage_history_query_.Handle(transitions);
    }

    [[nodiscard]] Contracts::RollbackDecisionView GetRollbackDecision(
        const Domain::RollbackDecision& decision) const {
        return rollback_query_.Handle(decision);
    }

    [[nodiscard]] std::vector<Contracts::WorkflowTimelineEntry> GetWorkflowTimeline(
        const std::vector<Domain::StageTransitionRecord>& transitions,
        const std::vector<Domain::RollbackDecision>& decisions) const {
        return timeline_query_.Handle(transitions, decisions);
    }

   private:
    Commands::CreateWorkflowHandler create_handler_{};
    Commands::AdvanceStageHandler advance_handler_{};
    Commands::RetryStageHandler retry_handler_{};
    Commands::RequestRollbackHandler request_rollback_handler_{};
    Commands::PerformRollbackHandler perform_rollback_handler_{};
    Commands::AbortWorkflowHandler abort_handler_{};
    Commands::CompleteWorkflowArchiveHandler archive_handler_{};
    Queries::GetWorkflowRunHandler workflow_run_query_{};
    Queries::GetWorkflowStageHistoryHandler stage_history_query_{};
    Queries::GetRollbackDecisionHandler rollback_query_{};
    Queries::GetWorkflowTimelineHandler timeline_query_{};
};

}  // namespace Siligen::Workflow::Application::Facade
