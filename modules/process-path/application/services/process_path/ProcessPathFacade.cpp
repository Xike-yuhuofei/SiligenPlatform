#include "application/services/process_path/ProcessPathFacade.h"

#include "../../../domain/trajectory/domain-services/GeometryNormalizer.h"
#include "../../../domain/trajectory/domain-services/ProcessAnnotator.h"
#include "../../../domain/trajectory/domain-services/TopologyRepairService.h"
#include "../../../domain/trajectory/domain-services/TrajectoryShaper.h"

namespace {

using ContractsContourElement = Siligen::ProcessPath::Contracts::ContourElement;
using ContractsContourElementType = Siligen::ProcessPath::Contracts::ContourElementType;
using ContractsNormalizedPath = Siligen::ProcessPath::Contracts::NormalizedPath;
using ContractsNormalizationConfig = Siligen::ProcessPath::Contracts::NormalizationConfig;
using ContractsPath = Siligen::ProcessPath::Contracts::Path;
using ContractsPrimitive = Siligen::ProcessPath::Contracts::Primitive;
using ContractsPrimitiveType = Siligen::ProcessPath::Contracts::PrimitiveType;
using ContractsProcessConfig = Siligen::ProcessPath::Contracts::ProcessConfig;
using ContractsProcessConstraint = Siligen::ProcessPath::Contracts::ProcessConstraint;
using ContractsProcessPath = Siligen::ProcessPath::Contracts::ProcessPath;
using ContractsProcessSegment = Siligen::ProcessPath::Contracts::ProcessSegment;
using ContractsProcessTag = Siligen::ProcessPath::Contracts::ProcessTag;
using ContractsSegment = Siligen::ProcessPath::Contracts::Segment;
using ContractsSegmentType = Siligen::ProcessPath::Contracts::SegmentType;
using ContractsTrajectoryShaperConfig = Siligen::ProcessPath::Contracts::TrajectoryShaperConfig;
using DomainContourElement = Siligen::Domain::Trajectory::ValueObjects::ContourElement;
using DomainContourElementType = Siligen::Domain::Trajectory::ValueObjects::ContourElementType;
using DomainNormalizedPath = Siligen::Domain::Trajectory::DomainServices::NormalizedPath;
using DomainNormalizationConfig = Siligen::Domain::Trajectory::DomainServices::NormalizationConfig;
using DomainPath = Siligen::Domain::Trajectory::ValueObjects::Path;
using DomainPrimitive = Siligen::Domain::Trajectory::ValueObjects::Primitive;
using DomainPrimitiveType = Siligen::Domain::Trajectory::ValueObjects::PrimitiveType;
using DomainProcessConfig = Siligen::Domain::Trajectory::ValueObjects::ProcessConfig;
using DomainProcessConstraint = Siligen::Domain::Trajectory::ValueObjects::ProcessConstraint;
using DomainProcessPath = Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using DomainProcessSegment = Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using DomainProcessTag = Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using DomainSegment = Siligen::Domain::Trajectory::ValueObjects::Segment;
using DomainSegmentType = Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using DomainTrajectoryShaperConfig = Siligen::Domain::Trajectory::DomainServices::TrajectoryShaperConfig;

DomainContourElementType ToDomainContourElementType(const ContractsContourElementType type) {
    return static_cast<DomainContourElementType>(type);
}

ContractsContourElementType ToContractContourElementType(const DomainContourElementType type) {
    return static_cast<ContractsContourElementType>(type);
}

DomainPrimitiveType ToDomainPrimitiveType(const ContractsPrimitiveType type) {
    return static_cast<DomainPrimitiveType>(type);
}

ContractsPrimitiveType ToContractPrimitiveType(const DomainPrimitiveType type) {
    return static_cast<ContractsPrimitiveType>(type);
}

DomainSegmentType ToDomainSegmentType(const ContractsSegmentType type) {
    return static_cast<DomainSegmentType>(type);
}

ContractsSegmentType ToContractSegmentType(const DomainSegmentType type) {
    return static_cast<ContractsSegmentType>(type);
}

DomainProcessTag ToDomainProcessTag(const ContractsProcessTag tag) {
    return static_cast<DomainProcessTag>(tag);
}

ContractsProcessTag ToContractProcessTag(const DomainProcessTag tag) {
    return static_cast<ContractsProcessTag>(tag);
}

DomainContourElement ToDomainContourElement(const ContractsContourElement& element) {
    DomainContourElement result;
    result.type = ToDomainContourElementType(element.type);
    result.line = {element.line.start, element.line.end};
    result.arc = {element.arc.center,
                  element.arc.radius,
                  element.arc.start_angle_deg,
                  element.arc.end_angle_deg,
                  element.arc.clockwise};
    result.spline.control_points = element.spline.control_points;
    result.spline.closed = element.spline.closed;
    return result;
}

ContractsContourElement ToContractContourElement(const DomainContourElement& element) {
    ContractsContourElement result;
    result.type = ToContractContourElementType(element.type);
    result.line = {element.line.start, element.line.end};
    result.arc = {element.arc.center,
                  element.arc.radius,
                  element.arc.start_angle_deg,
                  element.arc.end_angle_deg,
                  element.arc.clockwise};
    result.spline.control_points = element.spline.control_points;
    result.spline.closed = element.spline.closed;
    return result;
}

DomainPrimitive ToDomainPrimitive(const ContractsPrimitive& primitive) {
    DomainPrimitive result;
    result.type = ToDomainPrimitiveType(primitive.type);
    result.line = {primitive.line.start, primitive.line.end};
    result.arc = {primitive.arc.center,
                  primitive.arc.radius,
                  primitive.arc.start_angle_deg,
                  primitive.arc.end_angle_deg,
                  primitive.arc.clockwise};
    result.spline.control_points = primitive.spline.control_points;
    result.spline.closed = primitive.spline.closed;
    result.circle = {primitive.circle.center,
                     primitive.circle.radius,
                     primitive.circle.start_angle_deg,
                     primitive.circle.clockwise};
    result.ellipse = {primitive.ellipse.center,
                      primitive.ellipse.major_axis,
                      primitive.ellipse.ratio,
                      primitive.ellipse.start_param,
                      primitive.ellipse.end_param,
                      primitive.ellipse.clockwise};
    result.point.position = primitive.point.position;
    result.contour.closed = primitive.contour.closed;
    result.contour.elements.reserve(primitive.contour.elements.size());
    for (const auto& element : primitive.contour.elements) {
        result.contour.elements.push_back(ToDomainContourElement(element));
    }
    return result;
}

DomainNormalizationConfig ToDomainNormalizationConfig(const ContractsNormalizationConfig& config) {
    DomainNormalizationConfig result;
    result.unit_scale = config.unit_scale;
    result.continuity_tolerance = config.continuity_tolerance;
    result.approximate_splines = config.approximate_splines;
    result.spline_max_step_mm = config.spline_max_step_mm;
    result.spline_max_error_mm = config.spline_max_error_mm;
    return result;
}

DomainProcessConfig ToDomainProcessConfig(const ContractsProcessConfig& config) {
    DomainProcessConfig result;
    result.default_flow = config.default_flow;
    result.lead_on_dist = config.lead_on_dist;
    result.lead_off_dist = config.lead_off_dist;
    result.corner_slowdown = config.corner_slowdown;
    result.corner_angle_deg = config.corner_angle_deg;
    result.curve_chain_angle_deg = config.curve_chain_angle_deg;
    result.curve_chain_max_segment_mm = config.curve_chain_max_segment_mm;
    result.rapid_gap_threshold = config.rapid_gap_threshold;
    result.start_speed_factor = config.start_speed_factor;
    result.end_speed_factor = config.end_speed_factor;
    result.corner_speed_factor = config.corner_speed_factor;
    result.rapid_speed_factor = config.rapid_speed_factor;
    return result;
}

DomainTrajectoryShaperConfig ToDomainTrajectoryShaperConfig(const ContractsTrajectoryShaperConfig& config) {
    DomainTrajectoryShaperConfig result;
    result.corner_smoothing_radius = config.corner_smoothing_radius;
    result.corner_max_deviation_mm = config.corner_max_deviation_mm;
    result.corner_min_radius_mm = config.corner_min_radius_mm;
    result.position_tolerance = config.position_tolerance;
    return result;
}

ContractsSegment ToContractSegment(const DomainSegment& segment) {
    ContractsSegment result;
    result.type = ToContractSegmentType(segment.type);
    result.line = {segment.line.start, segment.line.end};
    result.arc = {segment.arc.center,
                  segment.arc.radius,
                  segment.arc.start_angle_deg,
                  segment.arc.end_angle_deg,
                  segment.arc.clockwise};
    result.spline.control_points = segment.spline.control_points;
    result.spline.closed = segment.spline.closed;
    result.length = segment.length;
    result.curvature_radius = segment.curvature_radius;
    result.is_spline_approx = segment.is_spline_approx;
    result.is_point = segment.is_point;
    return result;
}

ContractsPath ToContractPath(const DomainPath& path) {
    ContractsPath result;
    result.closed = path.closed;
    result.segments.reserve(path.segments.size());
    for (const auto& segment : path.segments) {
        result.segments.push_back(ToContractSegment(segment));
    }
    return result;
}

ContractsNormalizedPath ToContractNormalizedPath(const DomainNormalizedPath& path) {
    ContractsNormalizedPath result;
    result.path = ToContractPath(path.path);
    result.report.discontinuity_count = path.report.discontinuity_count;
    result.report.closed = path.report.closed;
    result.report.invalid_unit_scale = path.report.invalid_unit_scale;
    result.report.skipped_spline_count = path.report.skipped_spline_count;
    result.report.point_primitive_count = path.report.point_primitive_count;
    result.report.consumable_segment_count = path.report.consumable_segment_count;
    return result;
}

ContractsProcessConstraint ToContractProcessConstraint(const DomainProcessConstraint& constraint) {
    ContractsProcessConstraint result;
    result.has_velocity_factor = constraint.has_velocity_factor;
    result.max_velocity_factor = constraint.max_velocity_factor;
    result.has_local_speed_hint = constraint.has_local_speed_hint;
    result.local_speed_factor = constraint.local_speed_factor;
    return result;
}

ContractsProcessSegment ToContractProcessSegment(const DomainProcessSegment& segment) {
    ContractsProcessSegment result;
    result.geometry = ToContractSegment(segment.geometry);
    result.dispense_on = segment.dispense_on;
    result.flow_rate = segment.flow_rate;
    result.constraint = ToContractProcessConstraint(segment.constraint);
    result.tag = ToContractProcessTag(segment.tag);
    return result;
}

ContractsProcessPath ToContractProcessPath(const DomainProcessPath& path) {
    ContractsProcessPath result;
    result.segments.reserve(path.segments.size());
    for (const auto& segment : path.segments) {
        result.segments.push_back(ToContractProcessSegment(segment));
    }
    return result;
}

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

    std::vector<DomainPrimitive> domain_primitives;
    domain_primitives.reserve(repair_result.primitives.size());
    for (const auto& primitive : repair_result.primitives) {
        domain_primitives.push_back(ToDomainPrimitive(primitive));
    }

    const auto normalized = normalizer.Normalize(domain_primitives, ToDomainNormalizationConfig(request.normalization));
    result.normalized = ToContractNormalizedPath(normalized);
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

    const auto process_path = annotator.Annotate(normalized.path, ToDomainProcessConfig(request.process));
    result.process_path = ToContractProcessPath(process_path);
    if (!result.normalized.path.segments.empty() && result.process_path.segments.empty()) {
        SetFailure(result,
                   Siligen::ProcessPath::Contracts::PathGenerationStage::ProcessAnnotation,
                   "process annotation produced no process segments");
        return result;
    }

    const auto shaped_path = shaper.Shape(process_path, ToDomainTrajectoryShaperConfig(request.shaping));
    result.shaped_path = ToContractProcessPath(shaped_path);
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
