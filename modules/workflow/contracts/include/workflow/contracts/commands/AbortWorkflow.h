#pragma once

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct AbortWorkflow {
    std::string request_id;
    std::string idempotency_key;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string issued_by;
    std::int64_t issued_at = 0;
    std::string abort_reason_code;
    std::string abort_reason_message;
    std::string abort_scope;
};

}  // namespace Siligen::Workflow::Contracts
