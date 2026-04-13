#pragma once

#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <string>

namespace Siligen::Workflow::Domain::Policies {

class StageStateProjectionPolicy {
   public:
    [[nodiscard]] static std::string ResolveCurrentStage(const Workflow::Domain::WorkflowRun& workflow_run,
                                                         const Workflow::Domain::StageTransitionRecord& transition) {
        if (!transition.to_stage.empty()) {
            return transition.to_stage;
        }
        if (!workflow_run.target_stage.empty()) {
            return workflow_run.target_stage;
        }
        return workflow_run.current_stage;
    }

    [[nodiscard]] static bool ShouldAdvanceLastCompletedStage(
        const Workflow::Domain::StageTransitionRecord& transition) {
        return transition.to_state != Workflow::Domain::WorkflowRunState::RollbackPending &&
               transition.to_state != Workflow::Domain::WorkflowRunState::PreflightBlocked &&
               transition.to_state != Workflow::Domain::WorkflowRunState::Archived;
    }
};

}  // namespace Siligen::Workflow::Domain::Policies
