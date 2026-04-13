#pragma once

#include "shared/types/Result.h"
#include "workflow/domain/workflow_run/WorkflowArtifactRefSet.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Domain::Ports {

struct WorkflowEventEnvelope {
    std::string event_id;
    std::string workflow_id;
    std::string stage_id;
    std::string event_name;
    std::string correlation_id;
    std::string causation_id;
    std::string summary;
    Workflow::Domain::WorkflowArtifactRefSet artifact_refs;
    std::int64_t occurred_at = 0;
};

class IWorkflowEventIngressPort {
   public:
    virtual ~IWorkflowEventIngressPort() = default;

    virtual Siligen::Shared::Types::Result<void> Publish(
        const WorkflowEventEnvelope& envelope) = 0;
};

}  // namespace Siligen::Workflow::Domain::Ports
