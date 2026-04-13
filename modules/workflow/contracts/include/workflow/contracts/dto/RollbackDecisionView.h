#pragma once

#include "workflow/contracts/dto/WorkflowArtifactRefSet.h"

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

enum class RollbackTarget {
    ToSourceAccepted = 0,
    ToGeometryReady,
    ToTopologyReady,
    ToFeaturesReady,
    ToProcessPlanned,
    ToCoordinatesResolved,
    ToAligned,
    ToProcessPathReady,
    ToMotionPlanned,
    ToTimingPlanned,
    ToPackageBuilt,
    ToPackageValidated,
};

enum class RollbackDecisionStatus {
    Requested = 0,
    Evaluating,
    Executing,
    Rejected,
    Performed,
    Failed,
};

struct RollbackDecisionView {
    std::string rollback_id;
    std::string workflow_id;
    RollbackTarget rollback_target = RollbackTarget::ToSourceAccepted;
    RollbackDecisionStatus decision_status = RollbackDecisionStatus::Requested;
    std::string reason_code;
    std::string reason_message;
    WorkflowArtifactRefSet affected_artifact_refs;
    std::int64_t requested_at = 0;
    std::int64_t resolved_at = 0;
};

}  // namespace Siligen::Workflow::Contracts
