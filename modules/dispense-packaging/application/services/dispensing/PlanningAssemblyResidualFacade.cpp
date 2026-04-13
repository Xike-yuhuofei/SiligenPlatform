#include "application/services/dispensing/PlanningAssemblyResidualFacade.h"

#include "application/services/dispensing/PlanningAssemblyResidualInternals.h"

namespace Siligen::Application::Services::Dispensing::Internal {

Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifactsResidual(
    const WorkflowAuthorityPreviewRequest& request) {
    auto result = AssembleAuthorityPreviewArtifacts(BuildAuthorityPreviewBuildInput(request));
    if (result.IsError()) {
        return Result<WorkflowAuthorityPreviewArtifacts>::Failure(result.GetError());
    }
    return Result<WorkflowAuthorityPreviewArtifacts>::Success(
        BuildWorkflowAuthorityPreviewArtifacts(result.Value()));
}

Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthorityResidual(
    const WorkflowExecutionAssemblyRequest& request) {
    auto result = AssembleExecutionArtifacts(BuildExecutionAssemblyBuildInput(request));
    if (result.IsError()) {
        return Result<WorkflowExecutionAssemblyResult>::Failure(result.GetError());
    }
    return Result<WorkflowExecutionAssemblyResult>::Success(
        BuildWorkflowExecutionAssemblyResult(result.Value()));
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
