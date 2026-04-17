#include "application/services/dispensing/PlanningAssemblyResidualInternals.h"

#include "application/services/dispensing/PlanningArtifactExportAssemblyService.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <sstream>
#include <unordered_map>

namespace Siligen::Application::Services::Dispensing::Internal {

using Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyInput;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportAssemblyService;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportGluePointMetadata;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportExecutionTriggerMetadata;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Shared::Types::Result;

namespace {

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
    bool authority_shared = false;
};

std::vector<TrajectoryPoint> BuildTrajectoryFromMotion(const MotionTrajectory& trajectory) {
    return ConvertMotionTrajectoryToTrajectoryPoints(trajectory);
}

const char* ToString(LayoutTriggerSourceKind source_kind) {
    switch (source_kind) {
        case LayoutTriggerSourceKind::Anchor:
            return "anchor";
        case LayoutTriggerSourceKind::Generated:
            return "generated";
        case LayoutTriggerSourceKind::SharedVertex:
            return "shared_vertex";
    }
    return "generated";
}

Result<void> ValidateExecutionStrategyFeasibility(
    DispensingExecutionStrategy requested_strategy,
    DispensingExecutionGeometryKind geometry_kind) {
    if (geometry_kind != DispensingExecutionGeometryKind::POINT &&
        requested_strategy == DispensingExecutionStrategy::STATIONARY_SHOT) {
        return Result<void>::Failure(Siligen::Shared::Types::Error(
            Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
            "requested_execution_strategy=stationary_shot only supports POINT geometry",
            "ExecutionAssemblyService"));
    }

    return Result<void>::Success();
}

ExecutionTrajectorySelection SelectExecutionTrajectory(const ExecutionPackageValidated& execution_package) {
    ExecutionTrajectorySelection selection;
    selection.motion_trajectory_points = BuildTrajectoryFromMotion(execution_package.execution_plan.motion_trajectory);
    if (!execution_package.execution_plan.interpolation_points.empty()) {
        selection.execution_trajectory = &execution_package.execution_plan.interpolation_points;
        selection.authority_shared = true;
    }
    return selection;
}

PlanningArtifactsAssemblyInput BuildExecutionPlanningInput(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path) {
    PlanningArtifactsAssemblyInput execution_input;
    execution_input.process_path = execution_process_path;
    execution_input.authority_process_path = input.authority_process_path;
    execution_input.motion_plan = input.motion_plan;
    execution_input.source_path = input.source_path;
    execution_input.dxf_filename = input.dxf_filename;
    execution_input.dispensing_velocity = input.dispensing_velocity;
    execution_input.acceleration = input.acceleration;
    execution_input.dispenser_interval_ms = input.dispenser_interval_ms;
    execution_input.dispenser_duration_ms = input.dispenser_duration_ms;
    execution_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    execution_input.valve_response_ms = input.valve_response_ms;
    execution_input.safety_margin_ms = input.safety_margin_ms;
    execution_input.min_interval_ms = input.min_interval_ms;
    execution_input.max_jerk = input.max_jerk;
    execution_input.sample_dt = input.sample_dt;
    execution_input.sample_ds = input.sample_ds;
    execution_input.spline_max_step_mm = input.spline_max_step_mm;
    execution_input.spline_max_error_mm = input.spline_max_error_mm;
    execution_input.estimated_time_s = input.estimated_time_s;
    execution_input.use_interpolation_planner = input.use_interpolation_planner;
    execution_input.interpolation_algorithm = input.interpolation_algorithm;
    execution_input.compensation_profile = input.compensation_profile;
    return execution_input;
}

}  // namespace

TriggerArtifacts BuildTriggerArtifactsFromAuthorityPreview(
    const AuthorityPreviewBuildResult& authority_preview) {
    TriggerArtifacts artifacts;
    artifacts.authority_trigger_layout = authority_preview.authority_trigger_layout;
    artifacts.authority_trigger_points = authority_preview.authority_trigger_points;
    artifacts.spacing_validation_groups = authority_preview.spacing_validation_groups;
    artifacts.positions = authority_preview.glue_points;
    artifacts.validation_classification = authority_preview.preview_validation_classification;
    artifacts.exception_reason = authority_preview.preview_exception_reason;
    artifacts.failure_reason = authority_preview.preview_failure_reason;
    artifacts.spacing_valid = authority_preview.preview_spacing_valid;
    artifacts.has_short_segment_exceptions = authority_preview.preview_has_short_segment_exceptions;
    artifacts.binding_ready = authority_preview.preview_binding_ready;
    artifacts.authority_trigger_layout.binding_ready = authority_preview.preview_binding_ready;
    for (const auto& trigger : authority_preview.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }
    return artifacts;
}

Result<ExecutionPackageValidated> BuildValidatedExecutionPackage(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const TriggerArtifacts& trigger_artifacts,
    ExecutionGenerationArtifacts generation_artifacts) {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " authority_segments=" << input.authority_process_path.segments.size()
            << " canonical_execution_segments=" << input.canonical_execution_process_path.segments.size()
            << " execution_segments=" << execution_process_path.segments.size()
            << " motion_points=" << input.motion_plan.points.size()
            << " preview_layout=" << input.authority_preview.authority_trigger_layout.layout_id;
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    ExecutionPackageBuilt built;
    built.execution_plan.geometry_kind = ResolveExecutionGeometryKind(execution_process_path);
    auto strategy_validation = ValidateExecutionStrategyFeasibility(
        input.requested_execution_strategy,
        built.execution_plan.geometry_kind);
    if (strategy_validation.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(strategy_validation.GetError());
    }
    built.execution_plan.execution_strategy = ResolveExecutionStrategy(
        input.requested_execution_strategy,
        built.execution_plan.geometry_kind,
        generation_artifacts,
        input.motion_plan);
    built.execution_plan.interpolation_segments = std::move(generation_artifacts.interpolation_segments);
    built.execution_plan.interpolation_points = std::move(generation_artifacts.interpolation_points);
    built.execution_plan.motion_trajectory = generation_artifacts.motion_trajectory.points.empty()
        ? input.motion_plan
        : std::move(generation_artifacts.motion_trajectory);
    built.execution_plan.trigger_distances_mm = trigger_artifacts.distances;
    built.execution_plan.trigger_interval_ms = trigger_artifacts.interval_ms;
    built.execution_plan.trigger_interval_mm = trigger_artifacts.interval_mm;
    built.execution_plan.total_length_mm = built.execution_plan.motion_trajectory.total_length > kEpsilon
        ? built.execution_plan.motion_trajectory.total_length
        : input.motion_plan.total_length > kEpsilon
            ? input.motion_plan.total_length
        : ComputeProcessPathLength(execution_process_path);
    built.total_length_mm = built.execution_plan.total_length_mm;
    built.estimated_time_s = input.estimated_time_s;
    built.source_path = input.source_path;
    built.source_fingerprint = input.authority_preview.authority_trigger_layout.layout_id;
    built.estimated_time_s =
        EstimateExecutionTime(BuildExecutionPlanningInput(input, execution_process_path), built);

    ExecutionPackageValidated execution_package(std::move(built));
    auto package_validation = execution_package.Validate();
    if (package_validation.IsError()) {
        return Result<ExecutionPackageValidated>::Failure(package_validation.GetError());
    }
    {
        std::ostringstream oss;
        oss << "trajectory_points=" << execution_package.execution_plan.motion_trajectory.points.size()
            << " interpolation_segments=" << execution_package.execution_plan.interpolation_segments.size()
            << " interpolation_points=" << execution_package.execution_plan.interpolation_points.size()
            << " geometry_kind=" << Siligen::Shared::Types::ToString(execution_package.execution_plan.geometry_kind)
            << " execution_strategy="
            << Siligen::Shared::Types::ToString(execution_package.execution_plan.execution_strategy);
        log_stage("execution_package_ready", oss.str());
    }

    return Result<ExecutionPackageValidated>::Success(std::move(execution_package));
}

void PopulateExecutionTrajectorySelection(
    const ExecutionPackageValidated& execution_package,
    ExecutionAssemblyBuildResult& result,
    bool& authority_shared) {
    const auto selection = SelectExecutionTrajectory(execution_package);
    authority_shared = selection.authority_shared;
    if (selection.execution_trajectory) {
        result.execution_trajectory_points = *selection.execution_trajectory;
    }
    result.interpolation_trajectory_points = execution_package.execution_plan.interpolation_points;
    result.motion_trajectory_points = selection.motion_trajectory_points;
}

Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest BuildExecutionExportRequest(
    const ExecutionAssemblyBuildInput& input,
    const ProcessPath& execution_process_path,
    const ExecutionAssemblyBuildResult& result) {
    PlanningArtifactExportAssemblyService export_assembly_service;
    PlanningArtifactExportAssemblyInput export_input;
    export_input.source_path = input.source_path;
    export_input.dxf_filename = input.dxf_filename;
    export_input.process_path = execution_process_path;
    export_input.glue_points = CollectAuthorityPositions(result.authority_trigger_layout);
    export_input.glue_distances_mm.reserve(result.authority_trigger_layout.trigger_points.size());
    export_input.glue_point_metadata.reserve(result.authority_trigger_layout.trigger_points.size());
    export_input.execution_trigger_metadata.reserve(result.authority_trigger_layout.bindings.size());
    std::unordered_map<std::string, const Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint*> triggers_by_id;
    triggers_by_id.reserve(result.authority_trigger_layout.trigger_points.size());
    std::unordered_map<std::string, const Siligen::Domain::Dispensing::ValueObjects::DispenseSpan*> spans_by_id;
    spans_by_id.reserve(result.authority_trigger_layout.spans.size());
    for (const auto& trigger : result.authority_trigger_layout.trigger_points) {
        triggers_by_id.emplace(trigger.trigger_id, &trigger);
    }
    for (const auto& span : result.authority_trigger_layout.spans) {
        spans_by_id.emplace(span.span_id, &span);
    }
    for (const auto& trigger : result.authority_trigger_layout.trigger_points) {
        export_input.glue_distances_mm.push_back(trigger.distance_mm_global);
        PlanningArtifactExportGluePointMetadata metadata;
        metadata.span_ref = trigger.span_ref;
        metadata.sequence_index_global = trigger.sequence_index_global;
        metadata.sequence_index_span = trigger.sequence_index_span;
        metadata.source_segment_index = trigger.source_segment_index;
        metadata.distance_mm_span = trigger.distance_mm_span;
        metadata.source_kind = ToString(trigger.source_kind);
        const auto span_it = spans_by_id.find(trigger.span_ref);
        if (span_it != spans_by_id.end() && span_it->second != nullptr) {
            const auto& span = *span_it->second;
            metadata.component_index = span.component_index;
            metadata.span_order_index = span.order_index;
            metadata.span_closed = span.closed;
            metadata.span_total_length_mm = span.total_length_mm;
            metadata.span_actual_spacing_mm = span.actual_spacing_mm;
            metadata.span_phase_mm = span.phase_mm;
            metadata.span_dispatch_type = Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type);
            metadata.span_split_reason = Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason);
        }
        export_input.glue_point_metadata.push_back(std::move(metadata));
    }
    for (const auto& binding : result.authority_trigger_layout.bindings) {
        if (!binding.bound) {
            continue;
        }

        PlanningArtifactExportExecutionTriggerMetadata metadata;
        metadata.trajectory_index = binding.interpolation_index;
        metadata.authority_trigger_ref = binding.trigger_ref;
        metadata.binding_match_error_mm = binding.match_error_mm;
        metadata.binding_monotonic = binding.monotonic;
        const auto trigger_it = triggers_by_id.find(binding.trigger_ref);
        if (trigger_it != triggers_by_id.end() && trigger_it->second != nullptr) {
            const auto& trigger = *trigger_it->second;
            metadata.authority_trigger_index = trigger.sequence_index_global;
            metadata.source_segment_index = trigger.source_segment_index;
            metadata.authority_distance_mm = trigger.distance_mm_global;
            metadata.span_ref = trigger.span_ref;
            const auto span_it = spans_by_id.find(trigger.span_ref);
            if (span_it != spans_by_id.end() && span_it->second != nullptr) {
                const auto& span = *span_it->second;
                metadata.component_index = span.component_index;
                metadata.span_order_index = span.order_index;
            }
        }
        export_input.execution_trigger_metadata.push_back(std::move(metadata));
    }
    export_input.execution_trajectory_points = result.execution_trajectory_points;
    export_input.interpolation_trajectory_points = result.interpolation_trajectory_points;
    export_input.motion_trajectory_points = result.motion_trajectory_points;
    return export_assembly_service.BuildRequest(export_input);
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
