#pragma once

#include <cstdint>

namespace Siligen::Workflow::Domain {

enum class WorkflowRunState : std::uint8_t {
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

}  // namespace Siligen::Workflow::Domain
