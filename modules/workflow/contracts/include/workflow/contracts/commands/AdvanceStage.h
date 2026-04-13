#pragma once

#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct AdvanceStage {
    std::string request_id;
    std::string idempotency_key;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string issued_by;
    std::int64_t issued_at = 0;
    std::string from_stage;
    std::string target_stage;
    WorkflowRunState expected_state = WorkflowRunState::Created;
    std::string trigger_reason;
};

}  // namespace Siligen::Workflow::Contracts
