#include "application/services/dispensing/PlanningAssemblyResidualInternals.h"

#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <ctime>
#include <sstream>

namespace Siligen::Application::Services::Dispensing::Internal {
namespace {

using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlanner;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlannerRequest;
using Siligen::Domain::Dispensing::DomainServices::TriggerPlanner;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

float32 ResolveSpacingMin(const PlanningArtifactsAssemblyInput& input, float32 target_spacing) {
    if (input.spacing_min_mm > 0.0f) {
        return input.spacing_min_mm;
    }
    (void)target_spacing;
    return 2.7f;
}

float32 ResolveSpacingMax(const PlanningArtifactsAssemblyInput& input, float32 target_spacing) {
    if (input.spacing_max_mm > 0.0f) {
        return input.spacing_max_mm;
    }
    (void)target_spacing;
    return 3.3f;
}

std::vector<AuthorityTriggerPoint> ConvertAuthorityTriggerPoints(const AuthorityTriggerLayout& layout) {
    std::vector<AuthorityTriggerPoint> points;
    points.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        AuthorityTriggerPoint point;
        point.position = trigger.position;
        point.trigger_distance_mm = trigger.distance_mm_global;
        point.segment_index = trigger.source_segment_index;
        point.short_segment_exception = false;
        point.shared_vertex = trigger.shared_vertex;
        points.push_back(point);
    }
    return points;
}

std::vector<SpacingValidationGroup> ConvertSpacingValidationGroups(const AuthorityTriggerLayout& layout) {
    std::vector<SpacingValidationGroup> groups;
    groups.reserve(layout.validation_outcomes.size());
    for (const auto& outcome : layout.validation_outcomes) {
        SpacingValidationGroup group;
        auto span_it = std::find_if(
            layout.spans.begin(),
            layout.spans.end(),
            [&](const auto& span) { return span.span_id == outcome.span_ref; });
        if (span_it != layout.spans.end() && !span_it->source_segment_indices.empty()) {
            group.segment_index = span_it->source_segment_indices.front();
            group.actual_spacing_mm = span_it->actual_spacing_mm;
        }
        group.points = outcome.points;
        group.short_segment_exception =
            outcome.classification == SpacingValidationClassification::PassWithException;
        group.within_window = outcome.classification == SpacingValidationClassification::Pass;
        groups.push_back(std::move(group));
    }
    return groups;
}

std::string ResolveValidationClassification(const AuthorityTriggerLayout& layout) {
    if (layout.validation_outcomes.empty()) {
        return layout.authority_ready ? "pass" : "fail";
    }
    bool has_exception = false;
    for (const auto& outcome : layout.validation_outcomes) {
        if (outcome.classification == SpacingValidationClassification::Fail) {
            return "fail";
        }
        if (outcome.classification == SpacingValidationClassification::PassWithException) {
            has_exception = true;
        }
    }
    return has_exception ? "pass_with_exception" : "pass";
}

std::string ResolveExceptionReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.exception_reason.empty()) {
            return outcome.exception_reason;
        }
    }
    return {};
}

std::string ResolveBlockingReason(const AuthorityTriggerLayout& layout) {
    for (const auto& outcome : layout.validation_outcomes) {
        if (!outcome.blocking_reason.empty()) {
            return outcome.blocking_reason;
        }
    }
    return {};
}

void LogAuthoritySpanDiagnostics(const AuthorityTriggerLayout& layout) {
    {
        std::ostringstream oss;
        oss << "authority_layout_diagnostic"
            << " layout_id=" << layout.layout_id
            << " dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(layout.dispatch_type)
            << " effective_components=" << layout.effective_component_count
            << " ignored_components=" << layout.ignored_component_count
            << " total_components=" << layout.components.size()
            << " spans=" << layout.spans.size();
        SILIGEN_LOG_INFO(oss.str());
    }
    for (const auto& component : layout.components) {
        std::ostringstream oss;
        oss << "authority_component_diagnostic"
            << " layout_id=" << layout.layout_id
            << " component_id=" << component.component_id
            << " component_index=" << component.component_index
            << " dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(component.dispatch_type)
            << " ignored=" << (component.ignored ? 1 : 0)
            << " ignored_reason=" << (component.ignored_reason.empty() ? "none" : component.ignored_reason)
            << " source_segments=" << component.source_segment_indices.size()
            << " span_refs=" << component.span_refs.size();
        SILIGEN_LOG_INFO(oss.str());
    }
    for (const auto& span : layout.spans) {
        const auto outcome_it = std::find_if(
            layout.validation_outcomes.begin(),
            layout.validation_outcomes.end(),
            [&](const auto& outcome) { return outcome.span_ref == span.span_id; });
        std::ostringstream oss;
        oss << "authority_span_diagnostic"
            << " layout_id=" << layout.layout_id
            << " layout_dispatch_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(layout.dispatch_type)
            << " component_id=" << span.component_id
            << " component_index=" << span.component_index
            << " component_dispatch_type="
            << Siligen::Domain::Dispensing::ValueObjects::ToString(span.dispatch_type)
            << " span_id=" << span.span_id
            << " order_index=" << span.order_index
            << " topology_type=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.topology_type)
            << " split_reason=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.split_reason)
            << " anchor_policy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.anchor_policy)
            << " phase_strategy=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.phase_strategy)
            << " curve_mode=" << static_cast<int>(span.curve_mode)
            << " closed=" << (span.closed ? 1 : 0)
            << " source_segments=" << span.source_segment_indices.size()
            << " strong_anchors=" << span.strong_anchors.size()
            << " candidate_corners=" << span.candidate_corner_count
            << " accepted_corners=" << span.accepted_corner_count
            << " suppressed_corners=" << span.suppressed_corner_count
            << " dense_corner_clusters=" << span.dense_corner_cluster_count
            << " anchor_constraints_satisfied=" << (span.anchor_constraints_satisfied ? 1 : 0)
            << " validation_state=" << Siligen::Domain::Dispensing::ValueObjects::ToString(span.validation_state)
            << " anchor_constraint_state="
            << (outcome_it != layout.validation_outcomes.end() && !outcome_it->anchor_constraint_state.empty()
                    ? outcome_it->anchor_constraint_state
                    : "not_applicable")
            << " total_length_mm=" << span.total_length_mm
            << " actual_spacing_mm=" << span.actual_spacing_mm
            << " phase_mm=" << span.phase_mm;
        if (!span.strong_anchors.empty()) {
            oss << " anchor_roles=";
            for (std::size_t index = 0; index < span.strong_anchors.size(); ++index) {
                if (index > 0) {
                    oss << ',';
                }
                oss << Siligen::Domain::Dispensing::ValueObjects::ToString(span.strong_anchors[index].role);
            }
        }
        SILIGEN_LOG_INFO(oss.str());
    }
}

bool ValidateGlueSpacing(
    const PlanningArtifactsAssemblyInput& input,
    TriggerArtifacts& artifacts,
    float32 target_spacing_mm) {
    if (artifacts.spacing_validation_groups.empty()) {
        artifacts.spacing_valid = false;
        if (artifacts.failure_reason.empty()) {
            artifacts.failure_reason = "authority spacing groups unavailable";
        }
        return false;
    }

    const float32 min_allowed = ResolveSpacingMin(input, target_spacing_mm);
    const float32 max_allowed = ResolveSpacingMax(input, target_spacing_mm);

    bool valid = true;
    std::size_t invalid_group_count = 0;
    std::size_t short_exception_count = 0;
    for (const auto& group : artifacts.spacing_validation_groups) {
        if (group.short_segment_exception) {
            ++short_exception_count;
            continue;
        }
        if (!group.within_window) {
            valid = false;
            ++invalid_group_count;
        }
    }

    artifacts.has_short_segment_exceptions = short_exception_count > 0;
    artifacts.spacing_valid = valid;
    if (!valid && artifacts.failure_reason.empty()) {
        std::ostringstream oss;
        oss << "spacing validation failed: invalid_groups=" << invalid_group_count
            << ", exceptions=" << short_exception_count
            << ", target=" << target_spacing_mm
            << ", window=[" << min_allowed << ',' << max_allowed << ']';
        artifacts.failure_reason = oss.str();
        SILIGEN_LOG_WARNING(artifacts.failure_reason);
    }
    return valid;
}

Result<TriggerArtifacts> BuildTriggerArtifacts(
    const ProcessPath& path,
    const PlanningArtifactsAssemblyInput& input) {
    TriggerArtifacts artifacts;
    if (path.segments.empty()) {
        artifacts.failure_reason = "process path is empty";
        return Result<TriggerArtifacts>::Success(std::move(artifacts));
    }

    AuthorityTriggerLayoutPlanner planner;
    AuthorityTriggerLayoutPlannerRequest request;
    request.process_path = path;
    request.plan_id = input.source_path;
    request.plan_fingerprint = input.dxf_filename;
    request.layout_id_seed = input.source_path + "|" + input.dxf_filename + "|" + std::to_string(path.segments.size());
    request.target_spacing_mm = input.trigger_spatial_interval_mm;
    request.min_spacing_mm = ResolveSpacingMin(input, input.trigger_spatial_interval_mm);
    request.max_spacing_mm = ResolveSpacingMax(input, input.trigger_spatial_interval_mm);
    request.dispensing_velocity = input.dispensing_velocity;
    request.acceleration = input.acceleration;
    request.dispenser_interval_ms = input.dispenser_interval_ms;
    request.dispenser_duration_ms = input.dispenser_duration_ms;
    request.valve_response_ms = input.valve_response_ms;
    request.safety_margin_ms = input.safety_margin_ms;
    request.min_interval_ms = input.min_interval_ms;
    request.downgrade_on_violation = input.downgrade_on_violation;
    request.dispensing_strategy = input.dispensing_strategy;
    request.subsegment_count = input.subsegment_count;
    request.dispense_only_cruise = input.dispense_only_cruise;
    request.enable_branch_revisit_split = true;
    request.emit_topology_diagnostics = true;
    request.compensation_profile = input.compensation_profile;
    request.spline_max_error_mm = input.spline_max_error_mm;
    request.spline_max_step_mm = input.spline_max_step_mm;

    auto layout_result = planner.Plan(request);
    if (layout_result.IsError()) {
        return Result<TriggerArtifacts>::Failure(layout_result.GetError());
    }

    artifacts.authority_trigger_layout = layout_result.Value();
    artifacts.authority_trigger_points = ConvertAuthorityTriggerPoints(artifacts.authority_trigger_layout);
    artifacts.spacing_validation_groups = ConvertSpacingValidationGroups(artifacts.authority_trigger_layout);
    artifacts.positions = CollectAuthorityPositions(artifacts.authority_trigger_layout);
    artifacts.validation_classification = ResolveValidationClassification(artifacts.authority_trigger_layout);
    artifacts.exception_reason = ResolveExceptionReason(artifacts.authority_trigger_layout);
    artifacts.failure_reason = ResolveBlockingReason(artifacts.authority_trigger_layout);
    LogAuthoritySpanDiagnostics(artifacts.authority_trigger_layout);

    for (const auto& trigger : artifacts.authority_trigger_points) {
        artifacts.distances.push_back(trigger.trigger_distance_mm);
    }

    TriggerPlan trigger_plan;
    trigger_plan.strategy = input.dispensing_strategy;
    trigger_plan.interval_mm = input.trigger_spatial_interval_mm;
    trigger_plan.subsegment_count = input.subsegment_count;
    trigger_plan.dispense_only_cruise = input.dispense_only_cruise;
    trigger_plan.safety.duration_ms = static_cast<int32>(input.dispenser_duration_ms);
    trigger_plan.safety.valve_response_ms = static_cast<int32>(input.valve_response_ms);
    trigger_plan.safety.margin_ms = static_cast<int32>(input.safety_margin_ms);
    trigger_plan.safety.min_interval_ms = static_cast<int32>(input.min_interval_ms);
    trigger_plan.safety.downgrade_on_violation = input.downgrade_on_violation;

    TriggerPlanner trigger_planner;
    for (const auto& span : artifacts.authority_trigger_layout.spans) {
        auto timing_result = trigger_planner.Plan(
            span.total_length_mm,
            input.dispensing_velocity,
            input.acceleration,
            input.trigger_spatial_interval_mm,
            input.dispenser_interval_ms,
            0.0f,
            trigger_plan,
            input.compensation_profile);
        if (timing_result.IsError()) {
            return Result<TriggerArtifacts>::Failure(timing_result.GetError());
        }
        const auto& trigger_result = timing_result.Value();
        artifacts.interval_ms = std::max<uint32>(artifacts.interval_ms, trigger_result.interval_ms);
        artifacts.interval_mm = std::max(artifacts.interval_mm, trigger_result.plan.interval_mm);
        artifacts.downgraded = artifacts.downgraded || trigger_result.downgrade_applied;
    }

    if (artifacts.interval_mm <= kEpsilon) {
        artifacts.interval_mm = input.trigger_spatial_interval_mm;
    }
    if (artifacts.interval_ms == 0) {
        artifacts.interval_ms = input.dispenser_interval_ms;
    }
    if (artifacts.authority_trigger_points.empty() && artifacts.failure_reason.empty()) {
        artifacts.failure_reason = "explicit trigger authority unavailable";
    }
    ValidateGlueSpacing(input, artifacts, artifacts.interval_mm);
    return Result<TriggerArtifacts>::Success(std::move(artifacts));
}

}  // namespace

Result<AuthorityPreviewBuildResult> AssembleAuthorityPreviewArtifacts(
    const AuthorityPreviewBuildInput& input) {
    auto log_stage = [&](const char* stage, const std::string& detail = std::string()) {
        std::ostringstream oss;
        oss << "planning_artifacts_stage=" << stage
            << " dxf=" << input.dxf_filename
            << " process_segments=" << input.process_path.segments.size()
            << " preview_mode=authority_only";
        if (!detail.empty()) {
            oss << ' ' << detail;
        }
        SILIGEN_LOG_INFO(oss.str());
    };

    log_stage("start");

    if (input.process_path.segments.empty()) {
        return Result<AuthorityPreviewBuildResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path为空", "DispensePackagingAssembly"));
    }

    const auto& authority_process_path = ResolveAuthorityProcessPath(input);

    PlanningArtifactsAssemblyInput authority_input;
    authority_input.process_path = input.process_path;
    authority_input.authority_process_path = input.authority_process_path;
    authority_input.source_path = input.source_path;
    authority_input.dxf_filename = input.dxf_filename;
    authority_input.dispensing_velocity = input.dispensing_velocity;
    authority_input.acceleration = input.acceleration;
    authority_input.dispenser_interval_ms = input.dispenser_interval_ms;
    authority_input.dispenser_duration_ms = input.dispenser_duration_ms;
    authority_input.trigger_spatial_interval_mm = input.trigger_spatial_interval_mm;
    authority_input.valve_response_ms = input.valve_response_ms;
    authority_input.safety_margin_ms = input.safety_margin_ms;
    authority_input.min_interval_ms = input.min_interval_ms;
    authority_input.sample_dt = input.sample_dt;
    authority_input.sample_ds = input.sample_ds;
    authority_input.spline_max_step_mm = input.spline_max_step_mm;
    authority_input.spline_max_error_mm = input.spline_max_error_mm;
    authority_input.dispensing_strategy = input.dispensing_strategy;
    authority_input.subsegment_count = input.subsegment_count;
    authority_input.dispense_only_cruise = input.dispense_only_cruise;
    authority_input.downgrade_on_violation = input.downgrade_on_violation;
    authority_input.compensation_profile = input.compensation_profile;
    authority_input.spacing_tol_ratio = input.spacing_tol_ratio;
    authority_input.spacing_min_mm = input.spacing_min_mm;
    authority_input.spacing_max_mm = input.spacing_max_mm;

    auto trigger_artifacts_result = BuildTriggerArtifacts(authority_process_path, authority_input);
    if (trigger_artifacts_result.IsError()) {
        return Result<AuthorityPreviewBuildResult>::Failure(trigger_artifacts_result.GetError());
    }
    auto trigger_artifacts = trigger_artifacts_result.Value();
    {
        std::ostringstream oss;
        oss << "authority_points=" << trigger_artifacts.authority_trigger_points.size()
            << " layout_id=" << trigger_artifacts.authority_trigger_layout.layout_id
            << " validation=" << trigger_artifacts.validation_classification
            << " spacing_valid=" << (trigger_artifacts.spacing_valid ? 1 : 0)
            << " failure_reason=" << trigger_artifacts.failure_reason;
        log_stage("trigger_artifacts_ready", oss.str());
    }

    std::vector<TrajectoryPoint> preview_trajectory_points;
    auto preview_points_result =
        BuildInterpolationSeedPoints(
            input.process_path,
            input.spline_max_error_mm,
            ResolveInterpolationStep(authority_input));
    if (preview_points_result.IsError()) {
        if (trigger_artifacts.validation_classification == "fail") {
            std::ostringstream oss;
            oss << "preview_polyline_seed_fallback"
                << " layout_id=" << trigger_artifacts.authority_trigger_layout.layout_id
                << " reason=" << preview_points_result.GetError().GetMessage();
            log_stage("preview_polyline_failed_non_blocking", oss.str());
        } else {
            return Result<AuthorityPreviewBuildResult>::Failure(preview_points_result.GetError());
        }
    } else {
        preview_trajectory_points = ConvertPreviewPointsToTrajectory(preview_points_result.Value());
    }
    {
        std::ostringstream oss;
        oss << "preview_points=" << preview_trajectory_points.size();
        log_stage("preview_polyline_ready", oss.str());
    }

    const auto glue_points = CollectAuthorityPositions(trigger_artifacts.authority_trigger_layout);
    const auto total_length = ComputeProcessPathLength(input.process_path);

    AuthorityPreviewBuildResult result;
    result.segment_count = static_cast<int>(input.process_path.segments.size());
    result.total_length = total_length;
    result.estimated_time = EstimatePreviewTime(input, total_length);
    result.preview_trajectory_points = std::move(preview_trajectory_points);
    result.glue_points = glue_points;
    result.trigger_count = static_cast<int>(glue_points.size());
    result.dxf_filename = input.dxf_filename;
    result.timestamp = static_cast<int32>(std::time(nullptr));
    result.preview_authority_ready = !trigger_artifacts.authority_trigger_points.empty() && !glue_points.empty();
    result.preview_binding_ready = result.preview_authority_ready;
    result.preview_spacing_valid = trigger_artifacts.spacing_valid;
    result.preview_has_short_segment_exceptions = trigger_artifacts.has_short_segment_exceptions;
    result.preview_validation_classification = trigger_artifacts.validation_classification;
    result.preview_exception_reason = trigger_artifacts.exception_reason;
    result.preview_failure_reason = trigger_artifacts.failure_reason;
    if (!result.preview_authority_ready && result.preview_failure_reason.empty()) {
        result.preview_failure_reason = trigger_artifacts.authority_trigger_points.empty()
            ? "explicit trigger authority unavailable"
            : "authority preview unavailable";
    }
    result.authority_trigger_layout = trigger_artifacts.authority_trigger_layout;
    result.authority_trigger_points = trigger_artifacts.authority_trigger_points;
    result.spacing_validation_groups = trigger_artifacts.spacing_validation_groups;
    {
        std::ostringstream oss;
        oss << "preview_authority_ready=" << (result.preview_authority_ready ? 1 : 0)
            << " preview_binding_ready=" << (result.preview_binding_ready ? 1 : 0)
            << " preview_failure_reason=" << result.preview_failure_reason
            << " trigger_count=" << result.trigger_count;
        log_stage("complete", oss.str());
    }
    return Result<AuthorityPreviewBuildResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing::Internal
