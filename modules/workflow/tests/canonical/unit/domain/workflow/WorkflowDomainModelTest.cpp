#include "workflow/domain/policies/WorkflowCorrelationPolicy.h"
#include "workflow/domain/policies/WorkflowIdempotencyPolicy.h"
#include "workflow/domain/policies/WorkflowStateTransitionPolicy.h"
#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/rollback/RollbackPolicy.h"
#include "workflow/domain/stage_transition/StageStateProjectionPolicy.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Workflow::Domain::RollbackDecision;
using Siligen::Workflow::Domain::RollbackDecisionStatus;
using Siligen::Workflow::Domain::RollbackTarget;
using Siligen::Workflow::Domain::StageTransitionReason;
using Siligen::Workflow::Domain::StageTransitionRecord;
using Siligen::Workflow::Domain::WorkflowArchiveStatus;
using Siligen::Workflow::Domain::WorkflowRun;
using Siligen::Workflow::Domain::WorkflowRunState;
using Siligen::Workflow::Domain::Policies::RollbackPolicy;
using Siligen::Workflow::Domain::Policies::StageStateProjectionPolicy;
using Siligen::Workflow::Domain::Policies::WorkflowCorrelationPolicy;
using Siligen::Workflow::Domain::Policies::WorkflowIdempotencyPolicy;
using Siligen::Workflow::Domain::Policies::WorkflowStateTransitionPolicy;

}  // namespace

TEST(WorkflowDomainModelTest, StateTransitionPolicyMatchesCanonicalFlow) {
    EXPECT_TRUE(WorkflowStateTransitionPolicy::CanTransition(
        WorkflowRunState::Created,
        WorkflowRunState::SourceAccepted));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::CanTransition(
        WorkflowRunState::ReadyForPreflight,
        WorkflowRunState::PreflightBlocked));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::CanTransition(
        WorkflowRunState::Executing,
        WorkflowRunState::Completed));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::CanTransition(
        WorkflowRunState::Faulted,
        WorkflowRunState::Archived));
    EXPECT_FALSE(WorkflowStateTransitionPolicy::CanTransition(
        WorkflowRunState::Created,
        WorkflowRunState::Executing));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::AllowsRetry(WorkflowRunState::PreflightBlocked));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::AllowsRollback(WorkflowRunState::FirstArticlePending));
    EXPECT_TRUE(WorkflowStateTransitionPolicy::IsTerminal(WorkflowRunState::Archived));
}

TEST(WorkflowDomainModelTest, RollbackPolicyMapsFormalTargetBackToState) {
    EXPECT_EQ(
        RollbackPolicy::ResolveState(RollbackTarget::ToTimingPlanned),
        WorkflowRunState::TimingPlanned);
    EXPECT_TRUE(RollbackPolicy::CanRollbackFrom(WorkflowRunState::Executing));
    EXPECT_TRUE(RollbackPolicy::CanRollbackTo(
        WorkflowRunState::Executing,
        RollbackTarget::ToPackageValidated));
    EXPECT_FALSE(RollbackPolicy::CanRollbackTo(
        WorkflowRunState::ProcessPlanned,
        RollbackTarget::ToPackageValidated));
}

TEST(WorkflowDomainModelTest, WorkflowRunTracksArchiveAndPendingMetadata) {
    WorkflowRun workflow_run;
    workflow_run.workflow_id = "wf-1";
    workflow_run.job_id = "job-1";
    workflow_run.pending_request_id = "req-1";
    workflow_run.pending_command_id = "cmd-1";

    EXPECT_TRUE(workflow_run.HasPendingCommand());
    EXPECT_FALSE(workflow_run.IsTerminal());

    workflow_run.ApplyState(WorkflowRunState::Completed, "Completed", 10);
    workflow_run.MarkStageCompleted("Executing", 10);
    workflow_run.MarkArchiveRequested(11);
    workflow_run.MarkArchived(12);

    EXPECT_EQ(workflow_run.current_state, WorkflowRunState::Archived);
    EXPECT_EQ(workflow_run.archive_status, WorkflowArchiveStatus::Archived);
    EXPECT_EQ(workflow_run.last_completed_stage, "Executing");
    EXPECT_TRUE(workflow_run.IsTerminal());
    EXPECT_EQ(workflow_run.archived_at, 12);
}

TEST(WorkflowDomainModelTest, ProjectionAndCorrelationPoliciesUseCanonicalOwnerFields) {
    WorkflowRun workflow_run;
    workflow_run.workflow_id = "wf-1";
    workflow_run.job_id = "job-1";
    workflow_run.current_stage = "ProcessPlanned";
    workflow_run.target_stage = "CoordinatesResolved";
    workflow_run.pending_request_id = "req-1";

    StageTransitionRecord transition;
    transition.workflow_id = "wf-1";
    transition.from_state = WorkflowRunState::ProcessPlanned;
    transition.to_state = WorkflowRunState::CoordinatesResolved;
    transition.source_command_id = "cmd-1";
    transition.to_stage = "CoordinatesResolved";
    transition.transition_reason = StageTransitionReason::OwnerEventObserved;

    EXPECT_EQ(
        StageStateProjectionPolicy::ResolveCurrentStage(workflow_run, transition),
        "CoordinatesResolved");
    EXPECT_TRUE(StageStateProjectionPolicy::ShouldAdvanceLastCompletedStage(transition));
    EXPECT_TRUE(WorkflowCorrelationPolicy::MatchesWorkflow(workflow_run, "wf-1", "job-1"));
    EXPECT_TRUE(WorkflowCorrelationPolicy::MatchesTransition(transition, "wf-1", "cmd-1"));
    EXPECT_TRUE(WorkflowIdempotencyPolicy::HasStableKey("req-1", "idem-1"));
    EXPECT_TRUE(WorkflowIdempotencyPolicy::IsDuplicateRequest(workflow_run, "req-1"));
}

TEST(WorkflowDomainModelTest, RollbackDecisionTracksResolutionState) {
    RollbackDecision decision;
    decision.rollback_id = "rb-1";
    decision.workflow_id = "wf-1";
    decision.rollback_target = RollbackTarget::ToProcessPathReady;
    decision.decision_status = RollbackDecisionStatus::Evaluating;

    EXPECT_FALSE(decision.IsResolved());
    decision.MarkResolved(RollbackDecisionStatus::Performed, 20);
    EXPECT_TRUE(decision.IsResolved());
    EXPECT_EQ(decision.resolved_at, 20);
}
