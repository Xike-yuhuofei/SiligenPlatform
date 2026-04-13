#pragma once

#include "workflow/contracts/dto/RollbackDecisionView.h"
#include "workflow/contracts/failures/WorkflowFailureCategory.h"
#include "workflow/contracts/failures/WorkflowRecoveryAction.h"

#include <string>

namespace Siligen::Workflow::Contracts {

struct RollbackFailurePayload {
    WorkflowFailureCategory failure_category = WorkflowFailureCategory::RollbackPolicyViolation;
    std::string rollback_id;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    std::string failed_owner_module_id;
    std::string failure_code;
    std::string message;
    WorkflowRecoveryAction recommended_action = WorkflowRecoveryAction::AbortWorkflow;
};

}  // namespace Siligen::Workflow::Contracts
