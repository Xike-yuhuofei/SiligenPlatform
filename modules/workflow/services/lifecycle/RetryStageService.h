#pragma once

#include "workflow/contracts/commands/RetryStage.h"
#include "workflow/domain/stage_transition/StageTransitionReason.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Lifecycle {

class RetryStageService {
   public:
    [[nodiscard]] Domain::StageTransitionRecord PrepareRetryTransition(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RetryStage& command,
        std::int64_t occurred_at) const {
        Domain::StageTransitionRecord transition;
        transition.workflow_id = workflow_run.workflow_id;
        transition.from_state = workflow_run.current_state;
        transition.to_state = workflow_run.current_state;
        transition.from_stage = workflow_run.current_stage;
        transition.to_stage = command.stage_id;
        transition.source_command_id = command.request_id;
        transition.source_event_id = command.expected_failed_event_id;
        transition.transition_reason = Domain::StageTransitionReason::RetryStageRequested;
        transition.occurred_at = occurred_at;
        return transition;
    }
};

}  // namespace Siligen::Workflow::Services::Lifecycle
