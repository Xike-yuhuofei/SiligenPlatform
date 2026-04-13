#pragma once

#include "workflow/contracts/dto/RollbackDecisionView.h"
#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct RequestRollback {
    std::string request_id;
    std::string idempotency_key;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string issued_by;
    std::int64_t issued_at = 0;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    std::string reason_code;
    std::string reason_message;
    std::string requested_from_stage;
    WorkflowRunState requested_from_state = WorkflowRunState::Created;
};

}  // namespace Siligen::Workflow::Contracts
