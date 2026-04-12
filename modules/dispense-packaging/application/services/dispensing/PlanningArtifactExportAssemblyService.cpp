#include "application/services/dispensing/PlanningArtifactExportAssemblyService.h"

namespace Siligen::Application::Services::Dispensing {

Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest
PlanningArtifactExportAssemblyService::BuildRequest(
    const PlanningArtifactExportAssemblyInput& input) const {
    Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest request;
    request.source_path = input.source_path;
    request.dxf_filename = input.dxf_filename;
    request.process_path = input.process_path;
    request.glue_points = input.glue_points;
    request.execution_trajectory_points = input.execution_trajectory_points;
    request.interpolation_trajectory_points = input.interpolation_trajectory_points;
    request.motion_trajectory_points = input.motion_trajectory_points;
    return request;
}

}  // namespace Siligen::Application::Services::Dispensing
