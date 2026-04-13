#pragma once

#include "workflow/contracts/dto/RollbackDecisionView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct PerformRollback {
    std::string request_id;
    std::string idempotency_key;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string issued_by;
    std::int64_t issued_at = 0;
    std::string rollback_id;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    std::string supersede_scope;
};

}  // namespace Siligen::Workflow::Contracts
