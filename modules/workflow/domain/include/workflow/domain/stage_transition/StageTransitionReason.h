#pragma once

#include <cstdint>

namespace Siligen::Workflow::Domain {

enum class StageTransitionReason : std::uint8_t {
    WorkflowCreated = 0,
    AdvanceStageRequested,
    RetryStageRequested,
    OwnerEventObserved,
    RollbackRequested,
    RollbackPerformed,
    WorkflowAborted,
    ArchiveRequested,
    ArchiveCompleted,
};

}  // namespace Siligen::Workflow::Domain
