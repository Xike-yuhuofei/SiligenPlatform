#pragma once

#include "workflow/contracts/dto/RollbackDecisionView.h"
#include "workflow/contracts/dto/StageTransitionRecordView.h"
#include "workflow/contracts/dto/WorkflowRunView.h"
#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/stage_transition/StageTransitionReason.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Projection {

class WorkflowProjectionService {
   public:
    [[nodiscard]] Contracts::WorkflowRunView BuildWorkflowRunView(
        const Domain::WorkflowRun& workflow_run) const {
        Contracts::WorkflowRunView view;
        view.workflow_id = workflow_run.workflow_id;
        view.job_id = workflow_run.job_id;
        view.current_state = static_cast<Contracts::WorkflowRunState>(workflow_run.current_state);
        view.current_stage = workflow_run.current_stage;
        view.target_stage = workflow_run.target_stage;
        view.last_completed_stage = workflow_run.last_completed_stage;
        view.archive_status = static_cast<Contracts::WorkflowArchiveStatus>(workflow_run.archive_status);
        view.updated_at = workflow_run.updated_at;
        for (const auto& artifact_ref : workflow_run.active_artifact_refs.items) {
            view.active_artifact_refs.items.push_back(ToContractArtifact(artifact_ref));
        }
        return view;
    }

    [[nodiscard]] Contracts::StageTransitionRecordView BuildStageTransitionView(
        const Domain::StageTransitionRecord& transition) const {
        Contracts::StageTransitionRecordView view;
        view.transition_id = transition.transition_id;
        view.from_state = static_cast<Contracts::WorkflowRunState>(transition.from_state);
        view.to_state = static_cast<Contracts::WorkflowRunState>(transition.to_state);
        view.from_stage = transition.from_stage;
        view.to_stage = transition.to_stage;
        view.source_command_id = transition.source_command_id;
        view.source_event_id = transition.source_event_id;
        if (transition.artifact_ref.has_value()) {
            view.artifact_ref = ToContractArtifact(transition.artifact_ref.value());
        }
        view.transition_reason = ToString(transition.transition_reason);
        view.occurred_at = transition.occurred_at;
        return view;
    }

    [[nodiscard]] Contracts::RollbackDecisionView BuildRollbackDecisionView(
        const Domain::RollbackDecision& decision) const {
        Contracts::RollbackDecisionView view;
        view.rollback_id = decision.rollback_id;
        view.workflow_id = decision.workflow_id;
        view.rollback_target = static_cast<Contracts::RollbackTarget>(decision.rollback_target);
        view.decision_status = static_cast<Contracts::RollbackDecisionStatus>(decision.decision_status);
        view.reason_code = decision.reason_code;
        view.reason_message = decision.reason_message;
        view.requested_at = decision.requested_at;
        view.resolved_at = decision.resolved_at;
        for (const auto& artifact_ref : decision.affected_artifact_refs.items) {
            view.affected_artifact_refs.items.push_back(ToContractArtifact(artifact_ref));
        }
        return view;
    }

   private:
    [[nodiscard]] static Contracts::WorkflowArtifactRef ToContractArtifact(
        const Domain::WorkflowArtifactRef& artifact_ref) {
        Contracts::WorkflowArtifactRef result;
        result.owner_module_id = artifact_ref.owner_module_id;
        result.artifact_type = artifact_ref.artifact_type;
        result.artifact_id = artifact_ref.artifact_id;
        result.artifact_version = artifact_ref.artifact_version;
        result.relation = artifact_ref.relation;
        return result;
    }

    [[nodiscard]] static const char* ToString(Domain::StageTransitionReason reason) {
        switch (reason) {
            case Domain::StageTransitionReason::WorkflowCreated:
                return "WorkflowCreated";
            case Domain::StageTransitionReason::AdvanceStageRequested:
                return "AdvanceStageRequested";
            case Domain::StageTransitionReason::RetryStageRequested:
                return "RetryStageRequested";
            case Domain::StageTransitionReason::OwnerEventObserved:
                return "OwnerEventObserved";
            case Domain::StageTransitionReason::RollbackRequested:
                return "RollbackRequested";
            case Domain::StageTransitionReason::RollbackPerformed:
                return "RollbackPerformed";
            case Domain::StageTransitionReason::WorkflowAborted:
                return "WorkflowAborted";
            case Domain::StageTransitionReason::ArchiveRequested:
                return "ArchiveRequested";
            case Domain::StageTransitionReason::ArchiveCompleted:
                return "ArchiveCompleted";
            default:
                return "WorkflowCreated";
        }
    }
};

}  // namespace Siligen::Workflow::Services::Projection
