#pragma once

namespace Siligen::Workflow::Contracts {

enum class WorkflowFailureCategory {
    Validation = 0,
    OwnerCommandRejected,
    OwnerEventTimeout,
    StateTransitionViolation,
    RollbackPolicyViolation,
    ArchiveHandshakeFailed,
    Infrastructure,
    Unknown,
};

}  // namespace Siligen::Workflow::Contracts
