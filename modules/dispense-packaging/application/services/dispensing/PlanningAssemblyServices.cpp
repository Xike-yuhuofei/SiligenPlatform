#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"
#include "application/services/dispensing/PlanningAssemblyResidualFacade.h"

namespace Siligen::Application::Services::Dispensing {

Siligen::Shared::Types::Result<WorkflowAuthorityPreviewArtifacts>
AuthorityPreviewAssemblyService::BuildAuthorityPreviewArtifacts(
    const WorkflowAuthorityPreviewRequest& input) const {
    return Internal::BuildAuthorityPreviewArtifactsResidual(input);
}

Siligen::Shared::Types::Result<WorkflowExecutionAssemblyResult>
ExecutionAssemblyService::BuildExecutionArtifactsFromAuthority(
    const WorkflowExecutionAssemblyRequest& input) const {
    return Internal::BuildExecutionArtifactsFromAuthorityResidual(input);
}

}  // namespace Siligen::Application::Services::Dispensing
