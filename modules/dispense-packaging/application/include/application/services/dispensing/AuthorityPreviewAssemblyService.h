#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"

namespace Siligen::Application::Services::Dispensing {

class AuthorityPreviewAssemblyService {
public:
    Siligen::Shared::Types::Result<WorkflowAuthorityPreviewArtifacts> BuildAuthorityPreviewArtifacts(
        const WorkflowAuthorityPreviewRequest& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
