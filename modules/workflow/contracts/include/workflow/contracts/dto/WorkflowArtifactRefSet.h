#pragma once

#include <string>
#include <vector>

namespace Siligen::Workflow::Contracts {

struct WorkflowArtifactRef {
    std::string owner_module_id;
    std::string artifact_type;
    std::string artifact_id;
    std::string artifact_version;
    std::string relation;
};

struct WorkflowArtifactRefSet {
    std::vector<WorkflowArtifactRef> items;
};

}  // namespace Siligen::Workflow::Contracts
