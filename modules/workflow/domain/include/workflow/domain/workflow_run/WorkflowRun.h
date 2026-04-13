#pragma once

#include "workflow/domain/workflow_run/WorkflowArchiveStatus.h"
#include "workflow/domain/workflow_run/WorkflowArtifactRefSet.h"
#include "workflow/domain/workflow_run/WorkflowRunState.h"

#include <cstdint>
#include <string>
#include <utility>

namespace Siligen::Workflow::Domain {

struct WorkflowRun {
    std::string workflow_id;
    std::string job_id;
    WorkflowRunState current_state = WorkflowRunState::Created;
    std::string current_stage = "Created";
    std::string target_stage;
    std::string last_completed_stage;
    WorkflowArtifactRefSet active_artifact_refs;
    std::string pending_command_id;
    std::string pending_request_id;
    std::string current_execution_context;
    WorkflowArchiveStatus archive_status = WorkflowArchiveStatus::None;
    std::uint64_t version = 0;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
    std::int64_t completed_at = 0;
    std::int64_t aborted_at = 0;
    std::int64_t faulted_at = 0;
    std::int64_t archived_at = 0;

    [[nodiscard]] bool HasPendingCommand() const {
        return !pending_command_id.empty();
    }

    [[nodiscard]] bool IsTerminal() const {
        return current_state == WorkflowRunState::Completed ||
               current_state == WorkflowRunState::Aborted ||
               current_state == WorkflowRunState::Faulted ||
               current_state == WorkflowRunState::Archived;
    }

    [[nodiscard]] bool CanRequestArchive() const {
        return current_state == WorkflowRunState::Completed ||
               current_state == WorkflowRunState::Aborted ||
               current_state == WorkflowRunState::Faulted;
    }

    void ApplyState(WorkflowRunState next_state,
                    std::string next_stage,
                    std::int64_t occurred_at) {
        current_state = next_state;
        if (!next_stage.empty()) {
            current_stage = std::move(next_stage);
        }
        updated_at = occurred_at;
        ++version;
    }

    void MarkStageCompleted(std::string completed_stage, std::int64_t occurred_at) {
        last_completed_stage = std::move(completed_stage);
        updated_at = occurred_at;
        ++version;
    }

    void MarkArchiveRequested(std::int64_t occurred_at) {
        archive_status = WorkflowArchiveStatus::Requested;
        updated_at = occurred_at;
        ++version;
    }

    void MarkArchived(std::int64_t occurred_at) {
        archive_status = WorkflowArchiveStatus::Archived;
        current_state = WorkflowRunState::Archived;
        current_stage = "Archived";
        archived_at = occurred_at;
        updated_at = occurred_at;
        ++version;
    }
};

}  // namespace Siligen::Workflow::Domain
