#pragma once

#include "shared/types/Result.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <string>

namespace Siligen::Workflow::Domain::Ports {

class IWorkflowRepository {
   public:
    virtual ~IWorkflowRepository() = default;

    virtual Siligen::Shared::Types::Result<Workflow::Domain::WorkflowRun> GetById(
        const std::string& workflow_id) const = 0;
    virtual Siligen::Shared::Types::Result<void> SaveNew(
        const Workflow::Domain::WorkflowRun& workflow_run) = 0;
    virtual Siligen::Shared::Types::Result<void> Update(
        const Workflow::Domain::WorkflowRun& workflow_run) = 0;
};

}  // namespace Siligen::Workflow::Domain::Ports
