#pragma once

#include "workflow/contracts/dto/RollbackDecisionView.h"
#include "workflow/contracts/failures/WorkflowFailureCategory.h"
#include "workflow/contracts/failures/WorkflowRecoveryAction.h"

#include <string>

namespace Siligen::Workflow::Contracts {

enum class WorkflowFailureSeverity {
    Info = 0,
    Warning,
    Error,
    Critical,
};

struct WorkflowFailurePayload {
    WorkflowFailureCategory failure_category = WorkflowFailureCategory::Unknown;
    std::string failure_code;
    WorkflowFailureSeverity severity = WorkflowFailureSeverity::Error;
    bool retryable = false;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    WorkflowRecoveryAction recommended_action = WorkflowRecoveryAction::AwaitOwnerResolution;
    std::string owner_module_id;
    std::string owner_event_id;
    std::string message;
};

}  // namespace Siligen::Workflow::Contracts
