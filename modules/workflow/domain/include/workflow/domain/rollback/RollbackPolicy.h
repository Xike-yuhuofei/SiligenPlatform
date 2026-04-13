#pragma once

#include "workflow/domain/rollback/RollbackTarget.h"
#include "workflow/domain/workflow_run/WorkflowRunState.h"

namespace Siligen::Workflow::Domain::Policies {

class RollbackPolicy {
   public:
    [[nodiscard]] static Workflow::Domain::WorkflowRunState ResolveState(
        Workflow::Domain::RollbackTarget target) {
        switch (target) {
            case Workflow::Domain::RollbackTarget::ToSourceAccepted:
                return Workflow::Domain::WorkflowRunState::SourceAccepted;
            case Workflow::Domain::RollbackTarget::ToGeometryReady:
                return Workflow::Domain::WorkflowRunState::GeometryReady;
            case Workflow::Domain::RollbackTarget::ToTopologyReady:
                return Workflow::Domain::WorkflowRunState::TopologyReady;
            case Workflow::Domain::RollbackTarget::ToFeaturesReady:
                return Workflow::Domain::WorkflowRunState::FeaturesReady;
            case Workflow::Domain::RollbackTarget::ToProcessPlanned:
                return Workflow::Domain::WorkflowRunState::ProcessPlanned;
            case Workflow::Domain::RollbackTarget::ToCoordinatesResolved:
                return Workflow::Domain::WorkflowRunState::CoordinatesResolved;
            case Workflow::Domain::RollbackTarget::ToAligned:
                return Workflow::Domain::WorkflowRunState::Aligned;
            case Workflow::Domain::RollbackTarget::ToProcessPathReady:
                return Workflow::Domain::WorkflowRunState::ProcessPathReady;
            case Workflow::Domain::RollbackTarget::ToMotionPlanned:
                return Workflow::Domain::WorkflowRunState::MotionPlanned;
            case Workflow::Domain::RollbackTarget::ToTimingPlanned:
                return Workflow::Domain::WorkflowRunState::TimingPlanned;
            case Workflow::Domain::RollbackTarget::ToPackageBuilt:
                return Workflow::Domain::WorkflowRunState::PackageBuilt;
            case Workflow::Domain::RollbackTarget::ToPackageValidated:
                return Workflow::Domain::WorkflowRunState::PackageValidated;
            default:
                return Workflow::Domain::WorkflowRunState::SourceAccepted;
        }
    }

    [[nodiscard]] static bool CanRollbackFrom(Workflow::Domain::WorkflowRunState state) {
        return state != Workflow::Domain::WorkflowRunState::Created &&
               state != Workflow::Domain::WorkflowRunState::SourceAccepted &&
               state != Workflow::Domain::WorkflowRunState::Archived;
    }

    [[nodiscard]] static bool CanRollbackTo(Workflow::Domain::WorkflowRunState current_state,
                                            Workflow::Domain::RollbackTarget target) {
        return StateRank(ResolveState(target)) < StateRank(current_state);
    }

   private:
    [[nodiscard]] static int StateRank(Workflow::Domain::WorkflowRunState state) {
        switch (state) {
            case Workflow::Domain::WorkflowRunState::Created:
                return 0;
            case Workflow::Domain::WorkflowRunState::SourceAccepted:
                return 1;
            case Workflow::Domain::WorkflowRunState::GeometryReady:
                return 2;
            case Workflow::Domain::WorkflowRunState::TopologyReady:
                return 3;
            case Workflow::Domain::WorkflowRunState::FeaturesReady:
                return 4;
            case Workflow::Domain::WorkflowRunState::ProcessPlanned:
                return 5;
            case Workflow::Domain::WorkflowRunState::CoordinatesResolved:
                return 6;
            case Workflow::Domain::WorkflowRunState::Aligned:
                return 7;
            case Workflow::Domain::WorkflowRunState::ProcessPathReady:
                return 8;
            case Workflow::Domain::WorkflowRunState::MotionPlanned:
                return 9;
            case Workflow::Domain::WorkflowRunState::TimingPlanned:
                return 10;
            case Workflow::Domain::WorkflowRunState::PackageBuilt:
                return 11;
            case Workflow::Domain::WorkflowRunState::PackageValidated:
                return 12;
            case Workflow::Domain::WorkflowRunState::ReadyForPreflight:
                return 13;
            case Workflow::Domain::WorkflowRunState::PreflightBlocked:
                return 14;
            case Workflow::Domain::WorkflowRunState::MachineReady:
                return 15;
            case Workflow::Domain::WorkflowRunState::FirstArticlePending:
                return 16;
            case Workflow::Domain::WorkflowRunState::RollbackPending:
                return 17;
            case Workflow::Domain::WorkflowRunState::ReadyForProduction:
                return 18;
            case Workflow::Domain::WorkflowRunState::Executing:
                return 19;
            case Workflow::Domain::WorkflowRunState::Paused:
                return 20;
            case Workflow::Domain::WorkflowRunState::Recovering:
                return 21;
            case Workflow::Domain::WorkflowRunState::Completed:
                return 22;
            case Workflow::Domain::WorkflowRunState::Aborted:
                return 23;
            case Workflow::Domain::WorkflowRunState::Faulted:
                return 24;
            case Workflow::Domain::WorkflowRunState::Archived:
                return 25;
            default:
                return -1;
        }
    }
};

}  // namespace Siligen::Workflow::Domain::Policies
