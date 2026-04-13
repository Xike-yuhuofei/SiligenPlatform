#include "runtime/orchestrators/WorkflowArchiveOrchestrator.h"
#include "runtime/orchestrators/WorkflowCommandOrchestrator.h"
#include "runtime/orchestrators/WorkflowEventOrchestrator.h"
#include "runtime/subscriptions/TraceEventsSubscription.h"
#include "workflow/application/facade/WorkflowFacade.h"

#include <gtest/gtest.h>

namespace {

using Siligen::Workflow::Application::Facade::WorkflowFacade;
using Siligen::Workflow::Contracts::AdvanceStage;
using Siligen::Workflow::Contracts::CompleteWorkflowArchive;
using Siligen::Workflow::Contracts::CreateWorkflow;
using Siligen::Workflow::Contracts::RequestRollback;
using Siligen::Workflow::Contracts::RollbackTarget;
using Siligen::Workflow::Contracts::StageSucceeded;
using Siligen::Workflow::Contracts::WorkflowRunState;
using Siligen::Workflow::Runtime::Orchestrators::WorkflowArchiveOrchestrator;
using Siligen::Workflow::Runtime::Orchestrators::WorkflowCommandOrchestrator;
using Siligen::Workflow::Runtime::Orchestrators::WorkflowEventOrchestrator;
using Siligen::Workflow::Runtime::Subscriptions::TraceEventsSubscription;
using DomainWorkflowRunState = Siligen::Workflow::Domain::WorkflowRunState;

}  // namespace

TEST(WorkflowSkeletonSurfaceTest, FacadeAndCommandOrchestratorShareCanonicalSkeleton) {
    WorkflowFacade facade;
    WorkflowCommandOrchestrator orchestrator;

    CreateWorkflow create_command;
    create_command.request_id = "req-create";
    create_command.workflow_id = "wf-1";
    create_command.job_id = "job-1";
    create_command.issued_at = 1;

    auto workflow_run = orchestrator.CreateWorkflow(create_command);
    EXPECT_EQ(workflow_run.workflow_id, "wf-1");
    EXPECT_EQ(workflow_run.current_stage, "Created");

    AdvanceStage advance_command;
    advance_command.request_id = "req-advance";
    advance_command.workflow_id = "wf-1";
    advance_command.target_stage = "GeometryReady";
    advance_command.expected_state = WorkflowRunState::SourceAccepted;

    auto transition = facade.AdvanceStage(workflow_run, advance_command, 2);
    EXPECT_EQ(transition.workflow_id, "wf-1");
    EXPECT_EQ(transition.to_stage, "GeometryReady");

    RequestRollback rollback_command;
    rollback_command.request_id = "req-rb";
    rollback_command.workflow_id = "wf-1";
    rollback_command.rollback_target = RollbackTarget::ToSourceAccepted;
    rollback_command.reason_code = "WF-001";

    auto rollback_decision = facade.RequestRollback(workflow_run, rollback_command, 3);
    EXPECT_EQ(rollback_decision.workflow_id, "wf-1");
    EXPECT_EQ(rollback_decision.reason_code, "WF-001");
}

TEST(WorkflowSkeletonSurfaceTest, EventAndArchiveOrchestratorsUpdateOwnerModel) {
    WorkflowFacade facade;
    WorkflowEventOrchestrator event_orchestrator;
    WorkflowArchiveOrchestrator archive_orchestrator;
    TraceEventsSubscription trace_subscription;

    CreateWorkflow create_command;
    create_command.request_id = "req-create";
    create_command.workflow_id = "wf-2";
    create_command.job_id = "job-2";
    create_command.issued_at = 1;

    auto workflow_run = facade.CreateWorkflow(create_command);
    workflow_run.current_state = DomainWorkflowRunState::Executing;
    workflow_run.current_stage = "Executing";

    StageSucceeded success_event;
    success_event.request_id = "req-finish";
    success_event.event_id = "evt-finish";
    success_event.workflow_id = "wf-2";
    success_event.completed_stage = "Completed";
    success_event.resulting_state = WorkflowRunState::Completed;
    success_event.occurred_at = 10;

    auto completed_run = event_orchestrator.OnStageSucceeded(workflow_run, success_event);
    EXPECT_EQ(completed_run.current_state, DomainWorkflowRunState::Completed);
    EXPECT_EQ(completed_run.last_completed_stage, "Completed");

    CompleteWorkflowArchive archive_command;
    archive_command.request_id = "req-archive";
    archive_command.workflow_id = "wf-2";
    archive_command.execution_record_ref = "trace://record/1";

    auto archive_requested = archive_orchestrator.RequestArchive(completed_run, archive_command, 11);
    EXPECT_EQ(archive_requested.pending_request_id, "req-archive");

    auto archived_run = trace_subscription.OnArchiveCompleted(archive_requested, 12);
    EXPECT_EQ(archived_run.current_state, DomainWorkflowRunState::Archived);
    EXPECT_EQ(archived_run.current_stage, "Archived");
}
