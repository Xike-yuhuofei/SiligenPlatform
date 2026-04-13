#pragma once

#include "workflow/contracts/commands/RequestRollback.h"
#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Rollback {

class RequestRollbackService {
   public:
    [[nodiscard]] Domain::RollbackDecision CreateDecision(
        const Domain::WorkflowRun& workflow_run,
        const Contracts::RequestRollback& command,
        std::int64_t occurred_at) const {
        Domain::RollbackDecision decision;
        decision.rollback_id = command.request_id;
        decision.workflow_id = workflow_run.workflow_id;
        decision.requested_from_state = workflow_run.current_state;
        decision.requested_from_stage = command.requested_from_stage.empty()
            ? workflow_run.current_stage
            : command.requested_from_stage;
        decision.rollback_target = static_cast<Domain::RollbackTarget>(command.rollback_target);
        decision.reason_code = command.reason_code;
        decision.reason_message = command.reason_message;
        decision.decision_status = Domain::RollbackDecisionStatus::Evaluating;
        decision.requested_by = command.issued_by;
        decision.requested_at = occurred_at;
        return decision;
    }
};

}  // namespace Siligen::Workflow::Services::Rollback
