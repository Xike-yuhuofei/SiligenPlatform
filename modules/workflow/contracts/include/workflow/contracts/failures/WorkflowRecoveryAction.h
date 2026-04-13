#pragma once

namespace Siligen::Workflow::Contracts {

enum class WorkflowRecoveryAction {
    RetryCurrentStage = 0,
    RequestRollback,
    AbortWorkflow,
    EscalateToOperator,
    AwaitOwnerResolution,
};

}  // namespace Siligen::Workflow::Contracts
