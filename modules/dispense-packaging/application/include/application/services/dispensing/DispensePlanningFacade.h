#pragma once

#include "application/services/dispensing/PlanningAssemblyTypes.h"
#include "shared/types/Result.h"

namespace Siligen::Application::Services::Dispensing {

class DispensePlanningFacade {
   public:
    Siligen::Shared::Types::Result<AuthorityPreviewBuildResult> BuildAuthorityPreviewArtifacts(
        const AuthorityPreviewBuildInput& input) const;
    Siligen::Shared::Types::Result<ExecutionAssemblyBuildResult> BuildExecutionArtifactsFromAuthority(
        const ExecutionAssemblyBuildInput& input) const;
    Siligen::Shared::Types::Result<PlanningArtifactsBuildResult> AssemblePlanningArtifacts(
        const PlanningArtifactsBuildInput& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing

