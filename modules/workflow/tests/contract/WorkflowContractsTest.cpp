#include "workflow/contracts/WorkflowContracts.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Workflow::Contracts::WorkflowFailureCategory;
using Siligen::Workflow::Contracts::WorkflowPlanningTriggerResponse;
using Siligen::Workflow::Contracts::WorkflowStageLifecycle;
using Siligen::Workflow::Contracts::WorkflowStageState;

TEST(WorkflowContractsTest, StageStateCarriesDiagnosticsAndExportFlags) {
    WorkflowStageState state;
    state.workflow_run_id = "run-42";
    state.lifecycle = WorkflowStageLifecycle::Failed;
    state.failure_category = WorkflowFailureCategory::Export;
    state.diagnostic_error_code = 17;
    state.diagnostic_message = "export failed";
    state.observed_at_ms = 123456;
    state.stage_duration_ms = 250;
    state.export_attempted = true;
    state.export_succeeded = false;

    EXPECT_EQ(state.workflow_run_id, "run-42");
    EXPECT_EQ(state.diagnostic_error_code, 17);
    EXPECT_EQ(state.diagnostic_message, "export failed");
    EXPECT_EQ(state.observed_at_ms, 123456);
    EXPECT_EQ(state.stage_duration_ms, 250U);
    EXPECT_TRUE(state.export_attempted);
    EXPECT_FALSE(state.export_succeeded);
}

TEST(WorkflowContractsTest, TriggerResponseCarriesBoundaryDiagnostics) {
    WorkflowPlanningTriggerResponse response;
    response.accepted = false;
    response.failure_category = WorkflowFailureCategory::Infrastructure;
    response.diagnostic_error_code = 99;
    response.diagnostic_message = "dependency missing";
    response.observed_at_ms = 999;
    response.stage_duration_ms = 32;
    response.export_attempted = true;
    response.export_succeeded = false;

    EXPECT_FALSE(response.accepted);
    EXPECT_EQ(response.failure_category, WorkflowFailureCategory::Infrastructure);
    EXPECT_EQ(response.diagnostic_error_code, 99);
    EXPECT_EQ(response.diagnostic_message, "dependency missing");
    EXPECT_EQ(response.observed_at_ms, 999);
    EXPECT_EQ(response.stage_duration_ms, 32U);
    EXPECT_TRUE(response.export_attempted);
    EXPECT_FALSE(response.export_succeeded);
}

}  // namespace
