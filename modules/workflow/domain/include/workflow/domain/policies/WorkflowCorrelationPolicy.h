#pragma once

#include "workflow/domain/stage_transition/StageTransitionRecord.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <string>

namespace Siligen::Workflow::Domain::Policies {

class WorkflowCorrelationPolicy {
   public:
    [[nodiscard]] static bool MatchesWorkflow(const Workflow::Domain::WorkflowRun& workflow_run,
                                              const std::string& workflow_id,
                                              const std::string& job_id) {
        return !workflow_id.empty() &&
               workflow_run.workflow_id == workflow_id &&
               (job_id.empty() || workflow_run.job_id == job_id);
    }

    [[nodiscard]] static bool MatchesTransition(
        const Workflow::Domain::StageTransitionRecord& transition,
        const std::string& workflow_id,
        const std::string& causation_id) {
        return transition.workflow_id == workflow_id &&
               (causation_id.empty() || transition.source_command_id == causation_id);
    }
};

}  // namespace Siligen::Workflow::Domain::Policies
