#pragma once

#include "workflow/contracts/events/StageSucceeded.h"
#include "workflow/domain/stage_transition/StageTransitionReason.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Lifecycle {

class HandleStageSucceededService {
   public:
    [[nodiscard]] Domain::StageTransitionRecord BuildTransition(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::StageSucceeded& event) const {
        Domain::StageTransitionRecord transition;
        transition.workflow_id = workflow_run.workflow_id;
        transition.from_state = workflow_run.current_state;
        transition.to_state = static_cast<Domain::WorkflowRunState>(event.resulting_state);
        transition.from_stage = workflow_run.current_stage;
        transition.to_stage = event.completed_stage;
        transition.source_event_id = event.event_id;
        transition.transition_reason = Domain::StageTransitionReason::OwnerEventObserved;
        transition.occurred_at = event.occurred_at;
        if (!event.artifact_ref.artifact_id.empty()) {
            transition.artifact_ref = ToDomainArtifact(event.artifact_ref);
        }
        return transition;
    }

    [[nodiscard]] Domain::WorkflowRun Apply(
        Domain::WorkflowRun workflow_run,
        const Contracts::StageSucceeded& event) const {
        workflow_run.ApplyState(
            static_cast<Domain::WorkflowRunState>(event.resulting_state),
            event.completed_stage,
            event.occurred_at);
        workflow_run.MarkStageCompleted(event.completed_stage, event.occurred_at);
        workflow_run.pending_command_id.clear();
        workflow_run.pending_request_id = event.request_id;
        if (!event.artifact_ref.artifact_id.empty()) {
            workflow_run.active_artifact_refs.Upsert(ToDomainArtifact(event.artifact_ref));
        }
        return workflow_run;
    }

   private:
    [[nodiscard]] static Domain::WorkflowArtifactRef ToDomainArtifact(
        const Contracts::WorkflowArtifactRef& artifact_ref) {
        Domain::WorkflowArtifactRef domain_ref;
        domain_ref.owner_module_id = artifact_ref.owner_module_id;
        domain_ref.artifact_type = artifact_ref.artifact_type;
        domain_ref.artifact_id = artifact_ref.artifact_id;
        domain_ref.artifact_version = artifact_ref.artifact_version;
        domain_ref.relation = artifact_ref.relation;
        return domain_ref;
    }
};

}  // namespace Siligen::Workflow::Services::Lifecycle
