#pragma once

#include "domain/trajectory/value-objects/ProcessPath.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "shared/types/TrajectoryTypes.h"

#include <string>
#include <vector>

namespace Siligen::Application::Services::Dispensing {

struct PlanningArtifactExportRequest {
    std::string source_path;
    std::string dxf_filename;
    Domain::Trajectory::ValueObjects::ProcessPath process_path;
    std::vector<TrajectoryPoint> execution_trajectory_points;
    std::vector<TrajectoryPoint> interpolation_trajectory_points;
    std::vector<TrajectoryPoint> motion_trajectory_points;
    std::vector<Shared::Types::Point2D> glue_points;
};

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
