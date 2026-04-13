#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"
#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct StageStarted {
    std::string event_id;
    std::string request_id;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string stage_id;
    WorkflowArtifactRef artifact_ref;
    std::string producer;
    std::int64_t occurred_at = 0;
    WorkflowRunState from_state = WorkflowRunState::Created;
    WorkflowRunState to_state = WorkflowRunState::Created;
    std::string from_stage;
    std::string to_stage;
    std::string trigger_command;
};

}  // namespace Siligen::Workflow::Contracts
