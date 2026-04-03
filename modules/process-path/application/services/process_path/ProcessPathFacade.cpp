#include "application/services/process_path/ProcessPathFacade.h"

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TopologyRepairService.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"

namespace Siligen::Application::Services::ProcessPath {

ProcessPathBuildResult ProcessPathFacade::Build(const ProcessPathBuildRequest& request) const {
    Siligen::Domain::Trajectory::DomainServices::GeometryNormalizer normalizer;
    Siligen::Domain::Trajectory::DomainServices::ProcessAnnotator annotator;
    Siligen::Domain::Trajectory::DomainServices::TopologyRepairService repair_service;
    Siligen::Domain::Trajectory::DomainServices::TrajectoryShaper shaper;

    ProcessPathBuildResult result;
    const auto repair_result = repair_service.Repair(
        request.primitives,
        request.metadata,
        request.normalization.continuity_tolerance,
        request.topology_repair);
    result.topology_diagnostics = repair_result.diagnostics;
    result.normalized = normalizer.Normalize(repair_result.primitives, request.normalization);
    result.process_path = annotator.Annotate(result.normalized.path, request.process);
    result.shaped_path = shaper.Shape(result.process_path, request.shaping);
    result.topology_diagnostics.discontinuity_after = result.normalized.report.discontinuity_count;
    result.topology_diagnostics.rapid_segment_count = 0;
    result.topology_diagnostics.dispense_fragment_count = 0;
    bool previous_dispense = false;
    bool has_previous = false;
    for (const auto& segment : result.process_path.segments) {
        if (!segment.dispense_on) {
            result.topology_diagnostics.rapid_segment_count += 1;
        }
        if (segment.dispense_on && (!has_previous || !previous_dispense)) {
            result.topology_diagnostics.dispense_fragment_count += 1;
        }
        previous_dispense = segment.dispense_on;
        has_previous = true;
    }
    if (result.topology_diagnostics.dispense_fragment_count > 1 ||
        result.topology_diagnostics.rapid_segment_count > 0) {
        result.topology_diagnostics.fragmentation_suspected = true;
    }
    return result;
}

}  // namespace Siligen::Application::Services::ProcessPath
