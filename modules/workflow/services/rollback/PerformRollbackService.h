#pragma once

#include "workflow/contracts/commands/PerformRollback.h"
#include "workflow/domain/rollback/RollbackDecision.h"
#include "workflow/domain/rollback/RollbackPolicy.h"
#include "workflow/domain/workflow_run/WorkflowRun.h"

#include <string>

namespace Siligen::Workflow::Services::Rollback {

class PerformRollbackService {
   public:
    [[nodiscard]] Domain::WorkflowRun Apply(
        Domain::WorkflowRun workflow_run,
        const Domain::RollbackDecision& decision,
        const Contracts::PerformRollback& command,
        std::int64_t occurred_at) const {
        const auto rollback_state = Domain::Policies::RollbackPolicy::ResolveState(decision.rollback_target);
        workflow_run.ApplyState(rollback_state, ResolveStageName(decision.rollback_target), occurred_at);
        workflow_run.pending_request_id = command.request_id;
        workflow_run.pending_command_id.clear();
        return workflow_run;
    }

   private:
    [[nodiscard]] static std::string ResolveStageName(Domain::RollbackTarget target) {
        switch (target) {
            case Domain::RollbackTarget::ToSourceAccepted:
                return "SourceAccepted";
            case Domain::RollbackTarget::ToGeometryReady:
                return "GeometryReady";
            case Domain::RollbackTarget::ToTopologyReady:
                return "TopologyReady";
            case Domain::RollbackTarget::ToFeaturesReady:
                return "FeaturesReady";
            case Domain::RollbackTarget::ToProcessPlanned:
                return "ProcessPlanned";
            case Domain::RollbackTarget::ToCoordinatesResolved:
                return "CoordinatesResolved";
            case Domain::RollbackTarget::ToAligned:
                return "Aligned";
            case Domain::RollbackTarget::ToProcessPathReady:
                return "ProcessPathReady";
            case Domain::RollbackTarget::ToMotionPlanned:
                return "MotionPlanned";
            case Domain::RollbackTarget::ToTimingPlanned:
                return "TimingPlanned";
            case Domain::RollbackTarget::ToPackageBuilt:
                return "PackageBuilt";
            case Domain::RollbackTarget::ToPackageValidated:
                return "PackageValidated";
            default:
                return "SourceAccepted";
        }
    }
};

}  // namespace Siligen::Workflow::Services::Rollback
