#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"
#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct StageTransitionRecordView {
    std::string transition_id;
    WorkflowRunState from_state = WorkflowRunState::Created;
    WorkflowRunState to_state = WorkflowRunState::Created;
    std::string from_stage;
    std::string to_stage;
    std::string source_command_id;
    std::string source_event_id;
    WorkflowArtifactRef artifact_ref;
    std::string transition_reason;
    std::int64_t occurred_at = 0;
};

}  // namespace Siligen::Workflow::Contracts
