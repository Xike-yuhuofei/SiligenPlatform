#pragma once

#include "workflow/domain/stage_transition/StageTransitionReason.h"
#include "workflow/domain/workflow_run/WorkflowArtifactRefSet.h"
#include "workflow/domain/workflow_run/WorkflowRunState.h"

#include <cstdint>
#include <optional>
#include <string>

namespace Siligen::Workflow::Domain {

struct StageTransitionRecord {
    std::string transition_id;
    std::string workflow_id;
    WorkflowRunState from_state = WorkflowRunState::Created;
    WorkflowRunState to_state = WorkflowRunState::Created;
    std::string from_stage;
    std::string to_stage;
    std::string source_command_id;
    std::string source_event_id;
    std::optional<WorkflowArtifactRef> artifact_ref;
    std::string execution_context;
    StageTransitionReason transition_reason = StageTransitionReason::WorkflowCreated;
    std::int64_t occurred_at = 0;

    [[nodiscard]] bool HasEvidence() const {
        return !source_command_id.empty() || !source_event_id.empty();
    }
};

}  // namespace Siligen::Workflow::Domain
