#pragma once

#include "workflow/domain/workflow_run/WorkflowRunState.h"

namespace Siligen::Workflow::Domain::Policies {

class WorkflowStateTransitionPolicy {
   public:
    [[nodiscard]] static bool IsTerminal(Workflow::Domain::WorkflowRunState state) {
        return state == Workflow::Domain::WorkflowRunState::Completed ||
               state == Workflow::Domain::WorkflowRunState::Aborted ||
               state == Workflow::Domain::WorkflowRunState::Faulted ||
               state == Workflow::Domain::WorkflowRunState::Archived;
    }

    [[nodiscard]] static bool AllowsRetry(Workflow::Domain::WorkflowRunState state) {
        return state == Workflow::Domain::WorkflowRunState::PreflightBlocked ||
               state == Workflow::Domain::WorkflowRunState::RollbackPending;
    }

    [[nodiscard]] static bool AllowsRollback(Workflow::Domain::WorkflowRunState state) {
        return state != Workflow::Domain::WorkflowRunState::Created &&
               state != Workflow::Domain::WorkflowRunState::SourceAccepted &&
               state != Workflow::Domain::WorkflowRunState::Archived;
    }

    [[nodiscard]] static bool CanTransition(Workflow::Domain::WorkflowRunState from,
                                            Workflow::Domain::WorkflowRunState to) {
        if (from == to) {
            return true;
        }

        switch (from) {
            case Workflow::Domain::WorkflowRunState::Created:
                return to == Workflow::Domain::WorkflowRunState::SourceAccepted;
            case Workflow::Domain::WorkflowRunState::SourceAccepted:
                return to == Workflow::Domain::WorkflowRunState::GeometryReady;
            case Workflow::Domain::WorkflowRunState::GeometryReady:
                return to == Workflow::Domain::WorkflowRunState::TopologyReady;
            case Workflow::Domain::WorkflowRunState::TopologyReady:
                return to == Workflow::Domain::WorkflowRunState::FeaturesReady;
            case Workflow::Domain::WorkflowRunState::FeaturesReady:
                return to == Workflow::Domain::WorkflowRunState::ProcessPlanned;
            case Workflow::Domain::WorkflowRunState::ProcessPlanned:
                return to == Workflow::Domain::WorkflowRunState::CoordinatesResolved;
            case Workflow::Domain::WorkflowRunState::CoordinatesResolved:
                return to == Workflow::Domain::WorkflowRunState::Aligned;
            case Workflow::Domain::WorkflowRunState::Aligned:
                return to == Workflow::Domain::WorkflowRunState::ProcessPathReady;
            case Workflow::Domain::WorkflowRunState::ProcessPathReady:
                return to == Workflow::Domain::WorkflowRunState::MotionPlanned;
            case Workflow::Domain::WorkflowRunState::MotionPlanned:
                return to == Workflow::Domain::WorkflowRunState::TimingPlanned;
            case Workflow::Domain::WorkflowRunState::TimingPlanned:
                return to == Workflow::Domain::WorkflowRunState::PackageBuilt;
            case Workflow::Domain::WorkflowRunState::PackageBuilt:
                return to == Workflow::Domain::WorkflowRunState::PackageValidated;
            case Workflow::Domain::WorkflowRunState::PackageValidated:
                return to == Workflow::Domain::WorkflowRunState::ReadyForPreflight;
            case Workflow::Domain::WorkflowRunState::ReadyForPreflight:
                return to == Workflow::Domain::WorkflowRunState::MachineReady ||
                       to == Workflow::Domain::WorkflowRunState::PreflightBlocked;
            case Workflow::Domain::WorkflowRunState::PreflightBlocked:
                return to == Workflow::Domain::WorkflowRunState::ReadyForPreflight;
            case Workflow::Domain::WorkflowRunState::MachineReady:
                return to == Workflow::Domain::WorkflowRunState::FirstArticlePending ||
                       to == Workflow::Domain::WorkflowRunState::ReadyForProduction;
            case Workflow::Domain::WorkflowRunState::FirstArticlePending:
                return to == Workflow::Domain::WorkflowRunState::ReadyForProduction ||
                       to == Workflow::Domain::WorkflowRunState::RollbackPending;
            case Workflow::Domain::WorkflowRunState::RollbackPending:
                return to != Workflow::Domain::WorkflowRunState::Created &&
                       to != Workflow::Domain::WorkflowRunState::Archived;
            case Workflow::Domain::WorkflowRunState::ReadyForProduction:
                return to == Workflow::Domain::WorkflowRunState::Executing;
            case Workflow::Domain::WorkflowRunState::Executing:
                return to == Workflow::Domain::WorkflowRunState::Paused ||
                       to == Workflow::Domain::WorkflowRunState::Recovering ||
                       to == Workflow::Domain::WorkflowRunState::Completed ||
                       to == Workflow::Domain::WorkflowRunState::Aborted ||
                       to == Workflow::Domain::WorkflowRunState::Faulted;
            case Workflow::Domain::WorkflowRunState::Paused:
                return to == Workflow::Domain::WorkflowRunState::Executing ||
                       to == Workflow::Domain::WorkflowRunState::Aborted ||
                       to == Workflow::Domain::WorkflowRunState::Faulted;
            case Workflow::Domain::WorkflowRunState::Recovering:
                return to == Workflow::Domain::WorkflowRunState::Executing ||
                       to == Workflow::Domain::WorkflowRunState::Aborted ||
                       to == Workflow::Domain::WorkflowRunState::Faulted;
            case Workflow::Domain::WorkflowRunState::Completed:
            case Workflow::Domain::WorkflowRunState::Aborted:
            case Workflow::Domain::WorkflowRunState::Faulted:
                return to == Workflow::Domain::WorkflowRunState::Archived;
            case Workflow::Domain::WorkflowRunState::Archived:
                return false;
            default:
                return false;
        }
    }
};

}  // namespace Siligen::Workflow::Domain::Policies
