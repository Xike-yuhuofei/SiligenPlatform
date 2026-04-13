#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

enum class WorkflowRunState {
    Created = 0,
    SourceAccepted,
    GeometryReady,
    TopologyReady,
    FeaturesReady,
    ProcessPlanned,
    CoordinatesResolved,
    Aligned,
    ProcessPathReady,
    MotionPlanned,
    TimingPlanned,
    PackageBuilt,
    PackageValidated,
    ReadyForPreflight,
    PreflightBlocked,
    MachineReady,
    FirstArticlePending,
    RollbackPending,
    ReadyForProduction,
    Executing,
    Paused,
    Recovering,
    Completed,
    Aborted,
    Faulted,
    Archived,
};

enum class WorkflowArchiveStatus {
    None = 0,
    Requested,
    Archived,
    Failed,
};

struct WorkflowRunView {
    std::string workflow_id;
    std::string job_id;
    WorkflowRunState current_state = WorkflowRunState::Created;
    std::string current_stage = "Created";
    std::string target_stage;
    std::string last_completed_stage;
    WorkflowArtifactRefSet active_artifact_refs;
    WorkflowArchiveStatus archive_status = WorkflowArchiveStatus::None;
    std::int64_t updated_at = 0;
};

}  // namespace Siligen::Workflow::Contracts
