#pragma once

#include "application/services/dispensing/WorkflowPlanningAssemblyOperations.h"

#include <memory>

namespace Siligen::Application::Services::Dispensing {

class WorkflowPlanningAssemblyOperationsProvider {
public:
    std::shared_ptr<IWorkflowPlanningAssemblyOperations> CreateOperations() const;
};

}  // namespace Siligen::Application::Services::Dispensing
