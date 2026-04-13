#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"
#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct WorkflowCompleted {
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
    WorkflowRunState final_state = WorkflowRunState::Completed;
    std::string completion_source_event_id;
};

}  // namespace Siligen::Workflow::Contracts
