#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"
#include "shared/types/Result.h"

namespace Siligen::Application::Services::Dispensing {

class IWorkflowPlanningAssemblyOperations {
public:
    virtual ~IWorkflowPlanningAssemblyOperations() = default;

    virtual Siligen::Shared::Types::Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifacts(
        const WorkflowAuthorityPreviewRequest& input) const = 0;
    virtual Siligen::Shared::Types::Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthority(
        const WorkflowExecutionAssemblyRequest& input) const = 0;
    virtual Siligen::Shared::Types::Result<WorkflowPlanningAssemblyResult> AssemblePlanningArtifacts(
        const WorkflowPlanningAssemblyRequest& input) const = 0;
};

}  // namespace Siligen::Application::Services::Dispensing
