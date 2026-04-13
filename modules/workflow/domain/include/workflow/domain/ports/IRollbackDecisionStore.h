#pragma once

#include "shared/types/Result.h"
#include "workflow/domain/rollback/RollbackDecision.h"

#include <string>

namespace Siligen::Workflow::Domain::Ports {

class IRollbackDecisionStore {
   public:
    virtual ~IRollbackDecisionStore() = default;

    virtual Siligen::Shared::Types::Result<Workflow::Domain::RollbackDecision> GetById(
        const std::string& rollback_id) const = 0;
    virtual Siligen::Shared::Types::Result<void> SaveNew(
        const Workflow::Domain::RollbackDecision& rollback_decision) = 0;
    virtual Siligen::Shared::Types::Result<void> Update(
        const Workflow::Domain::RollbackDecision& rollback_decision) = 0;
};

}  // namespace Siligen::Workflow::Domain::Ports
