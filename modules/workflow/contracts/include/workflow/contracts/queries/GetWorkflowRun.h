#pragma once

#include <string>

namespace Siligen::Workflow::Contracts {

struct GetWorkflowRun {
    std::string workflow_id;
};

}  // namespace Siligen::Workflow::Contracts
