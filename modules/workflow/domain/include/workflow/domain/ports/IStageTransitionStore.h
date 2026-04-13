#pragma once

#include "shared/types/Result.h"
#include "workflow/domain/stage_transition/StageTransitionRecord.h"

#include <string>
#include <vector>

namespace Siligen::Workflow::Domain::Ports {

class IStageTransitionStore {
   public:
    virtual ~IStageTransitionStore() = default;

    virtual Siligen::Shared::Types::Result<void> Append(
        const Workflow::Domain::StageTransitionRecord& transition) = 0;
    virtual Siligen::Shared::Types::Result<std::vector<Workflow::Domain::StageTransitionRecord>> ListByWorkflow(
        const std::string& workflow_id) const = 0;
};

}  // namespace Siligen::Workflow::Domain::Ports
