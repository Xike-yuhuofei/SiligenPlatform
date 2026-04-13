#pragma once

#include <string>

namespace Siligen::Workflow::Contracts {

struct GetRollbackDecision {
    std::string workflow_id;
    std::string rollback_id;
};

}  // namespace Siligen::Workflow::Contracts
