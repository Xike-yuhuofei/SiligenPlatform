#pragma once

#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <string>

namespace Siligen::Workflow::Domain::Policies {

class WorkflowIdempotencyPolicy {
   public:
    [[nodiscard]] static bool HasStableKey(const std::string& request_id,
                                           const std::string& idempotency_key) {
        return !request_id.empty() && !idempotency_key.empty();
    }

    [[nodiscard]] static bool IsDuplicateRequest(const Workflow::Domain::WorkflowRun& workflow_run,
                                                 const std::string& request_id) {
        return !request_id.empty() &&
               !workflow_run.pending_request_id.empty() &&
               workflow_run.pending_request_id == request_id;
    }
};

}  // namespace Siligen::Workflow::Domain::Policies
