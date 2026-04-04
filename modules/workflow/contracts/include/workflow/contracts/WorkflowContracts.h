#pragma once

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

enum class WorkflowCommand {
    None = 0,
    PreparePlanning,
    PublishPlanningReady,
    StartExecution,
    RetryPlanning,
    CancelWorkflow,
    RequestRecovery,
};

enum class WorkflowFailureCategory {
    None = 0,
    Validation,
    InputPreparation,
    PlanningUnavailable,
    BoundaryViolation,
    Export,
    Infrastructure,
    Unknown,
};

enum class WorkflowRecoveryAction {
    None = 0,
    RetryCurrentStage,
    RequeueFromM0,
    EscalateToHost,
    AbortWorkflow,
};

enum class WorkflowStageLifecycle {
    Pending = 0,
    InProgress,
    Completed,
    Failed,
    Blocked,
    RecoveryPending,
};

struct WorkflowRecoveryDirective {
    WorkflowRecoveryAction action = WorkflowRecoveryAction::None;
    bool retryable = false;
    std::string reason;
};

struct WorkflowStageState {
    std::string workflow_run_id;
    std::string owner_artifact = "WorkflowRun";
    std::string source_artifact;
    std::string target_module_id = "M4";
    std::string stage_name;
    WorkflowCommand last_command = WorkflowCommand::None;
    WorkflowStageLifecycle lifecycle = WorkflowStageLifecycle::Pending;
    WorkflowFailureCategory failure_category = WorkflowFailureCategory::None;
    bool recovery_required = false;
    std::int32_t diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    std::uint32_t stage_duration_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

struct WorkflowPlanningTriggerRequest {
    std::string workflow_run_id;
    std::string source_artifact;
    std::string source_path;
    bool optimize_path = true;
    bool use_interpolation_planner = false;
};

struct WorkflowPlanningTriggerResponse {
    bool accepted = false;
    std::string prepared_input_path;
    WorkflowStageState stage_state;
    WorkflowFailureCategory failure_category = WorkflowFailureCategory::None;
    WorkflowRecoveryDirective recovery_directive;
    std::int32_t diagnostic_error_code = 0;
    std::string diagnostic_message;
    std::int64_t observed_at_ms = 0;
    std::uint32_t stage_duration_ms = 0;
    bool export_attempted = false;
    bool export_succeeded = false;
};

}  // namespace Siligen::Workflow::Contracts
