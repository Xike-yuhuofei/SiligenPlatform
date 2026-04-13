#pragma once

#include "workflow/domain/rollback/RollbackDecisionStatus.h"
#include "workflow/domain/rollback/RollbackTarget.h"
#include "workflow/domain/workflow_run/WorkflowArtifactRefSet.h"
#include "workflow/domain/workflow_run/WorkflowRunState.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Domain {

struct RollbackDecision {
    std::string rollback_id;
    std::string workflow_id;
    WorkflowRunState requested_from_state = WorkflowRunState::Created;
    std::string requested_from_stage;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    std::string reason_code;
    std::string reason_message;
    WorkflowArtifactRefSet affected_artifact_refs;
    RollbackDecisionStatus decision_status = RollbackDecisionStatus::Requested;
    std::string requested_by;
    std::int64_t requested_at = 0;
    std::int64_t resolved_at = 0;

    [[nodiscard]] bool IsResolved() const {
        return decision_status == RollbackDecisionStatus::Rejected ||
               decision_status == RollbackDecisionStatus::Performed ||
               decision_status == RollbackDecisionStatus::Failed;
    }

    void MarkResolved(RollbackDecisionStatus next_status, std::int64_t occurred_at) {
        decision_status = next_status;
        resolved_at = occurred_at;
    }
};

}  // namespace Siligen::Workflow::Domain
