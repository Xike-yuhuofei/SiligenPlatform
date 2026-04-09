#pragma once

#include "domain/dispensing/contracts/PlanningArtifactExportRequest.h"
#include "process_path/contracts/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/TrajectoryTypes.h"

#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

struct PlanningArtifactExportAssemblyInput {
    std::string source_path;
    std::string dxf_filename;
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    std::vector<Siligen::Shared::Types::Point2D> glue_points;
    std::vector<Siligen::TrajectoryPoint> execution_trajectory_points;
    std::vector<Siligen::TrajectoryPoint> interpolation_trajectory_points;
    std::vector<Siligen::TrajectoryPoint> motion_trajectory_points;
};

class PlanningArtifactExportAssemblyService {
public:
    PlanningArtifactExportRequest BuildRequest(const PlanningArtifactExportAssemblyInput& input) const;
};

}  // namespace Siligen::Application::Services::Dispensing
