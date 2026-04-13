#pragma once

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct CreateWorkflow {
    std::string request_id;
    std::string idempotency_key;
    std::string correlation_id;
    std::string causation_id;
    std::string workflow_id;
    std::string job_id;
    std::string issued_by;
    std::int64_t issued_at = 0;
    std::string initial_stage = "Created";
    std::string workflow_template_version;
    std::string context_snapshot_ref;
};

}  // namespace Siligen::Workflow::Contracts
