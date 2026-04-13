#include "workflow/contracts/WorkflowContracts.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace {

using Siligen::Workflow::Contracts::AdvanceStage;
using Siligen::Workflow::Contracts::CreateWorkflow;
using Siligen::Workflow::Contracts::WorkflowFailureCategory;
using Siligen::Workflow::Contracts::WorkflowFailurePayload;
using Siligen::Workflow::Contracts::WorkflowRecoveryAction;
using Siligen::Workflow::Contracts::WorkflowRunState;
using Siligen::Workflow::Contracts::RollbackDecisionStatus;
using Siligen::Workflow::Contracts::RollbackDecisionView;
using Siligen::Workflow::Contracts::RollbackTarget;
using Siligen::Workflow::Contracts::StageFailed;
using Siligen::Workflow::Contracts::StageTransitionRecordView;
using Siligen::Workflow::Contracts::WorkflowArchiveStatus;
using Siligen::Workflow::Contracts::WorkflowRunView;

std::filesystem::path RepoRoot() {
    auto path = std::filesystem::absolute(__FILE__);
    while (!path.empty() && !std::filesystem::exists(path / "modules")) {
        path = path.parent_path();
    }
    return path;
}

TEST(WorkflowContractsTest, CreateWorkflowCarriesCanonicalCommandMetadata) {
    CreateWorkflow command;
    command.request_id = "req-1";
    command.workflow_id = "wf-42";
    command.job_id = "job-7";
    command.workflow_template_version = "v1";
    command.context_snapshot_ref = "snapshot://ctx";

    EXPECT_EQ(command.request_id, "req-1");
    EXPECT_EQ(command.workflow_id, "wf-42");
    EXPECT_EQ(command.job_id, "job-7");
    EXPECT_EQ(command.initial_stage, "Created");
    EXPECT_EQ(command.workflow_template_version, "v1");
    EXPECT_EQ(command.context_snapshot_ref, "snapshot://ctx");
}

TEST(WorkflowContractsTest, StageFailedCarriesCanonicalFailurePayload) {
    StageFailed event;
    event.workflow_id = "wf-42";
    event.failed_stage = "TimingPlanned";
    event.resulting_state = WorkflowRunState::RollbackPending;
    event.failure.failure_category = WorkflowFailureCategory::RollbackPolicyViolation;
    event.failure.failure_code = "WF-ROLLBACK-001";
    event.failure.retryable = false;
    event.failure.rollback_target = RollbackTarget::ToProcessPathReady;
    event.failure.recommended_action = WorkflowRecoveryAction::RequestRollback;
    event.failure.message = "timing output superseded";

    EXPECT_EQ(event.workflow_id, "wf-42");
    EXPECT_EQ(event.failed_stage, "TimingPlanned");
    EXPECT_EQ(event.resulting_state, WorkflowRunState::RollbackPending);
    EXPECT_EQ(event.failure.failure_category, WorkflowFailureCategory::RollbackPolicyViolation);
    EXPECT_EQ(event.failure.rollback_target, RollbackTarget::ToProcessPathReady);
    EXPECT_EQ(event.failure.recommended_action, WorkflowRecoveryAction::RequestRollback);
    EXPECT_EQ(event.failure.message, "timing output superseded");
}

TEST(WorkflowContractsTest, WorkflowViewsUseTopLevelStateAndRollbackTypes) {
    WorkflowRunView run_view;
    run_view.workflow_id = "wf-42";
    run_view.current_state = WorkflowRunState::Executing;
    run_view.current_stage = "Executing";
    run_view.archive_status = WorkflowArchiveStatus::Requested;
    run_view.updated_at = 123456;

    StageTransitionRecordView transition_view;
    transition_view.transition_id = "tr-1";
    transition_view.from_state = WorkflowRunState::ReadyForProduction;
    transition_view.to_state = WorkflowRunState::Executing;
    transition_view.from_stage = "ReadyForProduction";
    transition_view.to_stage = "Executing";

    RollbackDecisionView rollback_view;
    rollback_view.rollback_id = "rb-1";
    rollback_view.workflow_id = "wf-42";
    rollback_view.rollback_target = RollbackTarget::ToPackageValidated;
    rollback_view.decision_status = RollbackDecisionStatus::Executing;

    AdvanceStage command;
    command.workflow_id = "wf-42";
    command.from_stage = "PackageValidated";
    command.target_stage = "ReadyForPreflight";
    command.expected_state = WorkflowRunState::PackageValidated;

    EXPECT_EQ(run_view.current_state, WorkflowRunState::Executing);
    EXPECT_EQ(run_view.archive_status, WorkflowArchiveStatus::Requested);
    EXPECT_EQ(transition_view.to_state, WorkflowRunState::Executing);
    EXPECT_EQ(rollback_view.rollback_target, RollbackTarget::ToPackageValidated);
    EXPECT_EQ(rollback_view.decision_status, RollbackDecisionStatus::Executing);
    EXPECT_EQ(command.expected_state, WorkflowRunState::PackageValidated);
}

TEST(WorkflowContractsTest, WorkflowContractsSurfaceDoesNotDependOnLegacyCompatibilityRoots) {
    const auto root = RepoRoot();

    EXPECT_FALSE(std::filesystem::exists(root / "modules/workflow/domain/domain"));
    EXPECT_FALSE(std::filesystem::exists(root / "modules/workflow/domain/include/domain"));
    EXPECT_FALSE(std::filesystem::exists(root / "modules/workflow/domain/motion-core"));
    EXPECT_FALSE(std::filesystem::exists(root / "modules/workflow/application/include/application"));
}

}  // namespace
