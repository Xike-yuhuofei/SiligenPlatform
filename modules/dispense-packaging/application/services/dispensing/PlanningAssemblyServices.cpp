#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"

#include "application/services/dispensing/PlanningAssemblyInternals.h"

namespace Siligen::Application::Services::Dispensing {
namespace {

using Siligen::Shared::Types::Result;

Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifactsImpl(
    const WorkflowAuthorityPreviewRequest& request) {
    auto result = Internal::AssembleAuthorityPreviewArtifacts(Internal::BuildAuthorityPreviewBuildInput(request));
    if (result.IsError()) {
        return Result<WorkflowAuthorityPreviewArtifacts>::Failure(result.GetError());
    }
    return Result<WorkflowAuthorityPreviewArtifacts>::Success(
        Internal::BuildWorkflowAuthorityPreviewArtifacts(result.Value()));
}

Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthorityImpl(
    const WorkflowExecutionAssemblyRequest& request) {
    auto result = Internal::AssembleExecutionArtifacts(Internal::BuildExecutionAssemblyBuildInput(request));
    if (result.IsError()) {
        return Result<WorkflowExecutionAssemblyResult>::Failure(result.GetError());
    }
    return Result<WorkflowExecutionAssemblyResult>::Success(
        Internal::BuildWorkflowExecutionAssemblyResult(result.Value()));
}

}  // namespace

Siligen::Shared::Types::Result<WorkflowAuthorityPreviewArtifacts>
AuthorityPreviewAssemblyService::BuildAuthorityPreviewArtifacts(
    const WorkflowAuthorityPreviewRequest& input) const {
    return BuildAuthorityPreviewArtifactsImpl(input);
}

Siligen::Shared::Types::Result<WorkflowExecutionAssemblyResult>
ExecutionAssemblyService::BuildExecutionArtifactsFromAuthority(
    const WorkflowExecutionAssemblyRequest& input) const {
    return BuildExecutionArtifactsFromAuthorityImpl(input);
}

}  // namespace Siligen::Application::Services::Dispensing
