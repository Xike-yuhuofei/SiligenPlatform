#pragma once

#include "workflow/contracts/dto/WorkflowRunView.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

enum class WorkflowTimelineEntryType {
    CommandIssued = 0,
    EventObserved,
    StateTransition,
    RollbackDecision,
    ArchiveHandshake,
};

struct WorkflowTimelineEntry {
    std::string entry_id;
    WorkflowTimelineEntryType entry_type = WorkflowTimelineEntryType::CommandIssued;
    std::string workflow_id;
    std::string stage_id;
    WorkflowRunState state = WorkflowRunState::Created;
    std::string source_command_id;
    std::string source_event_id;
    std::string summary;
    std::int64_t occurred_at = 0;
};

}  // namespace Siligen::Workflow::Contracts
