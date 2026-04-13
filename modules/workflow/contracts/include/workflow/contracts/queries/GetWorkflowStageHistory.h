#pragma once

#include <string>

namespace Siligen::Workflow::Contracts {

struct GetWorkflowStageHistory {
    std::string workflow_id;
    std::string stage_id;
};

}  // namespace Siligen::Workflow::Contracts
