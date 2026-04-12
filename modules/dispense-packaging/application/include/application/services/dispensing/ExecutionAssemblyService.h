#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyTypes.h"

namespace Siligen::Application::Services::Dispensing {

class ExecutionAssemblyService {
public:
    Siligen::Shared::Types::Result<WorkflowExecutionAssemblyResult> BuildExecutionArtifactsFromAuthority(
        const WorkflowExecutionAssemblyRequest& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
