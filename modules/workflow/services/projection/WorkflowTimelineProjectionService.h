#pragma once

#include "workflow/contracts/dto/WorkflowTimelineEntry.h"
#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"

namespace Siligen::Workflow::Services::Projection {

class WorkflowTimelineProjectionService {
   public:
    [[nodiscard]] Contracts::WorkflowTimelineEntry BuildFromTransition(
        const Domain::StageTransitionRecord& transition) const {
        Contracts::WorkflowTimelineEntry entry;
        entry.entry_id = transition.transition_id;
        entry.entry_type = Contracts::WorkflowTimelineEntryType::StateTransition;
        entry.workflow_id = transition.workflow_id;
        entry.stage_id = transition.to_stage;
        entry.state = static_cast<Contracts::WorkflowRunState>(transition.to_state);
        entry.source_command_id = transition.source_command_id;
        entry.source_event_id = transition.source_event_id;
        entry.summary = transition.to_stage;
        entry.occurred_at = transition.occurred_at;
        return entry;
    }

    [[nodiscard]] Contracts::WorkflowTimelineEntry BuildFromRollbackDecision(
        const Domain::RollbackDecision& decision) const {
        Contracts::WorkflowTimelineEntry entry;
        entry.entry_id = decision.rollback_id;
        entry.entry_type = Contracts::WorkflowTimelineEntryType::RollbackDecision;
        entry.workflow_id = decision.workflow_id;
        entry.stage_id = decision.requested_from_stage;
        entry.state = static_cast<Contracts::WorkflowRunState>(decision.requested_from_state);
        entry.summary = decision.reason_code;
        entry.occurred_at = decision.requested_at;
        return entry;
    }
};

}  // namespace Siligen::Workflow::Services::Projection
