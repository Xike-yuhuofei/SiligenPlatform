#pragma once

#include "shared/types/Result.h"
#include "workflow/domain/workflow_run/WorkflowArtifactRefSet.h"

#include <string>

namespace Siligen::Workflow::Domain::Ports {

struct StageOwnerCommandEnvelope {
    std::string workflow_id;
    std::string job_id;
    std::string target_stage;
    std::string command_name;
    std::string correlation_id;
    std::string causation_id;
    Workflow::Domain::WorkflowArtifactRefSet artifact_refs;
    std::string payload_ref;
};

class IStageOwnerCommandPort {
   public:
    virtual ~IStageOwnerCommandPort() = default;

    virtual Siligen::Shared::Types::Result<void> Dispatch(
        const StageOwnerCommandEnvelope& envelope) = 0;
};

}  // namespace Siligen::Workflow::Domain::Ports
