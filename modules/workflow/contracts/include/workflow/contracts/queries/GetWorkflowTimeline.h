#pragma once

#include <cstdint>
#include <string>

namespace Siligen::Workflow::Contracts {

struct GetWorkflowTimeline {
    std::string workflow_id;
    std::string stage_id;
    std::int64_t occurred_after = 0;
};

}  // namespace Siligen::Workflow::Contracts
