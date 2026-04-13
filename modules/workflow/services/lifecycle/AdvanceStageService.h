#pragma once

#include "workflow/contracts/commands/AdvanceStage.h"
#include "workflow/domain/stage_transition/StageTransitionReason.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Lifecycle {

class AdvanceStageService {
   public:
    [[nodiscard]] Domain::StageTransitionRecord PrepareTransition(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::AdvanceStage& command,
        std::int64_t occurred_at) const {
        Domain::StageTransitionRecord transition;
        transition.workflow_id = workflow_run.workflow_id;
        transition.from_state = workflow_run.current_state;
        transition.to_state = workflow_run.current_state;
        transition.from_stage = workflow_run.current_stage;
        transition.to_stage = command.target_stage;
        transition.source_command_id = command.request_id;
        transition.transition_reason = Domain::StageTransitionReason::AdvanceStageRequested;
        transition.occurred_at = occurred_at;
        return transition;
    }
};

}  // namespace Siligen::Workflow::Services::Lifecycle
