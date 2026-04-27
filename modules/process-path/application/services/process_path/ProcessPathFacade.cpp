#include "application/services/process_path/ProcessPathFacade.h"

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TopologyRepairService.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"

namespace {

using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::PathTopologyDiagnostics;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::PrimitiveType;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::TopologyRepairPolicy;

void SetFailure(Siligen::Application::Services::ProcessPath::ProcessPathBuildResult& result,
                const Siligen::ProcessPath::Contracts::PathGenerationStage stage,
                const char* const message) {
    result.status = PathGenerationStatus::StageFailure;
    result.failed_stage = stage;
    result.error_message = message;
}

void SetInvalidRepairMetadata(
    Siligen::Application::Services::ProcessPath::ProcessPathBuildResult& result,
    const std::size_t primitive_count) {
    result.status = PathGenerationStatus::InvalidInput;
    result.failed_stage = PathGenerationStage::InputValidation;
    result.error_message = "topology repair policy Auto requires metadata for every primitive";
    result.topology_diagnostics.repair_requested = true;
    result.topology_diagnostics.repair_applied = false;
    result.topology_diagnostics.metadata_valid = false;
    result.topology_diagnostics.original_primitive_count = static_cast<int>(primitive_count);
    result.topology_diagnostics.repaired_primitive_count = static_cast<int>(primitive_count);
}

void PopulateDispenseDiagnostics(const ProcessPath& path,
                                 int& rapid_segment_count,
                                 int& dispense_fragment_count,
                                 bool& fragmentation_suspected) {
    rapid_segment_count = 0;
    dispense_fragment_count = 0;
    bool previous_dispense = false;
    bool has_previous = false;
    for (const auto& segment : path.segments) {
        if (!segment.dispense_on) {
            rapid_segment_count += 1;
        }
        if (segment.dispense_on && (!has_previous || !previous_dispense)) {
            dispense_fragment_count += 1;
        }
        previous_dispense = segment.dispense_on;
        has_previous = true;
    }

    fragmentation_suspected =
        fragmentation_suspected || dispense_fragment_count > 1 || rapid_segment_count > 0;
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
        result.status = PathGenerationStatus::InvalidInput;
        result.failed_stage = PathGenerationStage::InputValidation;
        result.error_message = "path generation requires at least one primitive";
        return result;
    }
    if (request.alignment.has_value() && request.alignment->owner_module != "M5") {
        result.status = PathGenerationStatus::InvalidInput;
        result.failed_stage = PathGenerationStage::InputValidation;
        result.error_message = "alignment input must remain owned by M5";
        return result;
    }
    if (request.topology_repair.policy == TopologyRepairPolicy::Auto &&
        request.metadata.size() != request.primitives.size()) {
        SetInvalidRepairMetadata(result, request.primitives.size());
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
        result.status = PathGenerationStatus::InvalidInput;
        result.failed_stage = PathGenerationStage::Normalization;
        result.error_message = "normalization.unit_scale must be greater than zero";
        return result;
    }
    if (result.normalized.report.skipped_spline_count > 0) {
        SetFailure(result,
                   PathGenerationStage::Normalization,
                   "normalization rejected spline primitives because DXF input governance forbids SPLINE execution");
        return result;
    }
    if (result.normalized.report.consumable_segment_count == 0 &&
        result.normalized.report.point_primitive_count == 0) {
        SetFailure(result,
                   PathGenerationStage::Normalization,
                   "normalization produced no consumable path segments");
        return result;
    }
    if (!repair_result.primitives.empty() && result.normalized.path.segments.empty()) {
        SetFailure(result,
                   PathGenerationStage::Normalization,
                   "normalization produced no consumable path segments");
        return result;
    }

    const auto process_path = annotator.Annotate(normalized.path, request.process);
    result.process_path = process_path;
    if (!result.normalized.path.segments.empty() && result.process_path.segments.empty()) {
        SetFailure(result,
                   PathGenerationStage::ProcessAnnotation,
                   "process annotation produced no process segments");
        return result;
    }

    const auto shaped_path = shaper.Shape(process_path, request.shaping);
    result.shaped_path = shaped_path;
    if (!result.process_path.segments.empty() && result.shaped_path.segments.empty()) {
        SetFailure(result,
                   PathGenerationStage::TrajectoryShaping,
                   "trajectory shaping produced no shaped segments");
        return result;
    }

    result.status = PathGenerationStatus::Success;
    result.failed_stage = PathGenerationStage::None;
    result.error_message.clear();
    PopulateDispenseDiagnostics(result.process_path,
                                result.topology_diagnostics.pre_shape_rapid_segment_count,
                                result.topology_diagnostics.pre_shape_dispense_fragment_count,
                                result.topology_diagnostics.fragmentation_suspected);
    PopulateDispenseDiagnostics(result.shaped_path,
                                result.topology_diagnostics.rapid_segment_count,
                                result.topology_diagnostics.dispense_fragment_count,
                                result.topology_diagnostics.fragmentation_suspected);
    return result;
}

}  // namespace Siligen::Application::Services::ProcessPath
