#pragma once

#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

namespace Siligen::Workflow::Services::Rollback {

class RollbackImpactAnalysisService {
   public:
    [[nodiscard]] Domain::WorkflowArtifactRefSet Analyze(
        const Domain::WorkflowRun& workflow_run,
        Domain::RollbackTarget) const {
        return workflow_run.active_artifact_refs;
    }

    void Apply(Domain::RollbackDecision& decision, const Domain::WorkflowRun& workflow_run) const {
        decision.affected_artifact_refs = Analyze(workflow_run, decision.rollback_target);
    }
};

}  // namespace Siligen::Workflow::Services::Rollback
