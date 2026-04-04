#include "application/services/process_path/ProcessPathFacade.h"

#include "../../../domain/trajectory/domain-services/GeometryNormalizer.h"
#include "../../../domain/trajectory/domain-services/ProcessAnnotator.h"
#include "../../../domain/trajectory/domain-services/TopologyRepairService.h"
#include "../../../domain/trajectory/domain-services/TrajectoryShaper.h"

namespace {

void SetFailure(Siligen::Application::Services::ProcessPath::ProcessPathBuildResult& result,
                const Siligen::ProcessPath::Contracts::PathGenerationStage stage,
                const char* const message) {
    result.status = Siligen::ProcessPath::Contracts::PathGenerationStatus::StageFailure;
    result.failed_stage = stage;
    result.error_message = message;
}

}  // namespace

namespace Siligen::Application::Services::ProcessPath {

ProcessPathBuildResult ProcessPathFacade::Build(const ProcessPathBuildRequest& request) const {
    Siligen::Domain::Trajectory::DomainServices::GeometryNormalizer normalizer;
    Siligen::Domain::Trajectory::DomainServices::ProcessAnnotator annotator;
    Siligen::Domain::Trajectory::DomainServices::TopologyRepairService repair_service;
    Siligen::Domain::Trajectory::DomainServices::TrajectoryShaper shaper;

    ProcessPathBuildResult result;
    if (request.primitives.empty()) {
        result.status = Siligen::ProcessPath::Contracts::PathGenerationStatus::InvalidInput;
        result.failed_stage = Siligen::ProcessPath::Contracts::PathGenerationStage::InputValidation;
        result.error_message = "path generation requires at least one primitive";
        return result;
    }
    if (request.alignment.has_value() && request.alignment->owner_module != "M5") {
        result.status = Siligen::ProcessPath::Contracts::PathGenerationStatus::InvalidInput;
        result.failed_stage = Siligen::ProcessPath::Contracts::PathGenerationStage::InputValidation;
        result.error_message = "alignment input must remain owned by M5";
        return result;
    }

    const auto repair_result = repair_service.Repair(
        request.primitives,
        request.metadata,
        request.normalization.continuity_tolerance,
        request.topology_repair);
    result.topology_diagnostics = repair_result.diagnostics;

    const auto normalized = normalizer.Normalize(repair_result.primitives, request.normalization);
    result.normalized = normalized;
    result.topology_diagnostics.discontinuity_after = result.normalized.report.discontinuity_count;
    if (result.normalized.report.invalid_unit_scale) {
        result.status = Siligen::ProcessPath::Contracts::PathGenerationStatus::InvalidInput;
        result.failed_stage = Siligen::ProcessPath::Contracts::PathGenerationStage::Normalization;
        result.error_message = "normalization.unit_scale must be greater than zero";
        return result;
    }
    if (result.normalized.report.skipped_spline_count > 0) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::Normalization,
                   "normalization skipped spline primitives because approximate_splines is disabled");
        return result;
    }
    if (result.normalized.report.consumable_segment_count == 0) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::Normalization,
                   "normalization produced no consumable path segments");
        return result;
    }
    if (!repair_result.primitives.empty() && result.normalized.path.segments.empty()) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::Normalization,
                   "normalization produced no consumable path segments");
        return result;
    }

    const auto process_path = annotator.Annotate(normalized.path, request.process);
    result.process_path = process_path;
    if (!result.normalized.path.segments.empty() && result.process_path.segments.empty()) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::ProcessAnnotation,
                   "process annotation produced no process segments");
        return result;
    }

    const auto shaped_path = shaper.Shape(process_path, request.shaping);
    result.shaped_path = shaped_path;
    if (!result.process_path.segments.empty() && result.shaped_path.segments.empty()) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::TrajectoryShaping,
                   "trajectory shaping produced no shaped segments");
        return result;
    }

    result.status = Siligen::ProcessPath::Contracts::PathGenerationStatus::Success;
    result.failed_stage = Siligen::ProcessPath::Contracts::PathGenerationStage::None;
    result.error_message.clear();
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
