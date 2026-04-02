#pragma once

#include "domain/dispensing/contracts/PlanningArtifactExportRequest.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

struct PlanningArtifactExportResult {
    bool export_requested = false;
    bool success = true;
    std::vector<std::string> exported_paths;
    std::string message;
};

class IPlanningArtifactExportPort {
   public:
    virtual ~IPlanningArtifactExportPort() = default;

    virtual Shared::Types::Result<PlanningArtifactExportResult> Export(
        const PlanningArtifactExportRequest& request) = 0;
};

}  // namespace Siligen::Application::Services::Dispensing
