#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::Workflow::Domain {

struct WorkflowArtifactRef {
    std::string owner_module_id;
    std::string artifact_type;
    std::string artifact_id;
    std::string artifact_version;
    std::string relation;
};

struct WorkflowArtifactRefSet {
    std::vector<WorkflowArtifactRef> items;

    [[nodiscard]] bool Empty() const {
        return items.empty();
    }

    void Upsert(WorkflowArtifactRef artifact_ref) {
        const auto it = std::find_if(items.begin(), items.end(), [&](const WorkflowArtifactRef& current) {
            return current.owner_module_id == artifact_ref.owner_module_id &&
                   current.artifact_type == artifact_ref.artifact_type &&
                   current.relation == artifact_ref.relation;
        });
        if (it == items.end()) {
            items.push_back(std::move(artifact_ref));
            return;
        }

        *it = std::move(artifact_ref);
    }
};

}  // namespace Siligen::Workflow::Domain
