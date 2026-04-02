#pragma once

#include "application/services/dispensing/PlanningAssemblyTypes.h"

namespace Siligen::Application::Services::Dispensing {

class ExecutionAssemblyService {
public:
    Siligen::Shared::Types::Result<ExecutionAssemblyBuildResult> BuildExecutionArtifactsFromAuthority(
        const ExecutionAssemblyBuildInput& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
