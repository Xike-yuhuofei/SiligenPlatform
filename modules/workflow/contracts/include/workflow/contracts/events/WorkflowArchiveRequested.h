#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct WorkflowArchiveRequested {
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
    std::string archive_scope;
    std::string requested_execution_record_ref;
};

}  // namespace Siligen::Workflow::Contracts
