#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"
#include "shared/types/Result.h"

namespace Siligen::Application::Services::Dispensing::Internal {

Siligen::Shared::Types::Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifactsResidual(
    const WorkflowAuthorityPreviewRequest& request);

Siligen::Shared::Types::Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthorityResidual(
    const WorkflowExecutionAssemblyRequest& request);

}  // namespace Siligen::Application::Services::Dispensing::Internal
