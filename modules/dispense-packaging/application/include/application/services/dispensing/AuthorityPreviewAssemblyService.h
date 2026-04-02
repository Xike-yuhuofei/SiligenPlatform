#pragma once

#include "application/services/dispensing/PlanningAssemblyTypes.h"

namespace Siligen::Application::Services::Dispensing {

class AuthorityPreviewAssemblyService {
public:
    Siligen::Shared::Types::Result<AuthorityPreviewBuildResult> BuildAuthorityPreviewArtifacts(
        const AuthorityPreviewBuildInput& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
