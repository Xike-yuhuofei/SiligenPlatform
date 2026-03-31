#include "AuthorityTriggerLayoutPlanner.h"

#include "ClosedLoopAnchorConstraintSolver.h"
#include "ClosedLoopCornerAnchorResolver.h"
#include "ClosedLoopPlanningUtils.h"
#include "CurveFlatteningService.h"
#include "PathArcLengthLocator.h"
#include "TopologyComponentClassifier.h"
#include "TopologySpanSplitter.h"

#include "domain/dispensing/domain-services/TriggerPlanner.h"
#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityLayoutComponent;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpan;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanAnchorPolicy;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanCurveMode;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanPhaseStrategy;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanTopologyType;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationOutcome;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchor;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::Domain::Dispensing::ValueObjects::TopologyDispatchType;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentEnd;
using Siligen::Domain::Trajectory::ValueObjects::SegmentStart;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;

namespace {

constexpr float32 kEpsilon = 1e-6f;
constexpr float32 kDefaultSpacingMinMm = 2.7f;
constexpr float32 kDefaultSpacingMaxMm = 3.3f;
constexpr float32 kSharedVertexToleranceMm = 1e-4f;

float32 ResolveSpacingMin(const AuthorityTriggerLayoutPlannerRequest& request) {
    return request.min_spacing_mm > 0.0f ? request.min_spacing_mm : kDefaultSpacingMinMm;
}

float32 ResolveSpacingMax(const AuthorityTriggerLayoutPlannerRequest& request) {
    return request.max_spacing_mm > 0.0f ? request.max_spacing_mm : kDefaultSpacingMaxMm;
}

std::uint64_t Fnv1a64(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string BuildStableId(const std::string& prefix, const std::string& seed) {
    std::ostringstream oss;
    oss << prefix << '-' << std::hex << Fnv1a64(seed);
    return oss.str();
}

float32 ResolveVertexTolerance(const AuthorityTriggerLayoutPlannerRequest& request) {
    return request.topology_vertex_tolerance_mm > 0.0f
        ? request.topology_vertex_tolerance_mm
        : kSharedVertexToleranceMm;
}

float32 ResolveClosedLoopCornerClusterDistance(
    const AuthorityTriggerLayoutPlannerRequest& request,
    float32 min_spacing_mm) {
    return request.closed_loop_corner_cluster_distance_mm > 0.0f
        ? request.closed_loop_corner_cluster_distance_mm
        : min_spacing_mm;
}

float32 ResolveClosedLoopAnchorTolerance(
    const AuthorityTriggerLayoutPlannerRequest& request,
    float32 vertex_tolerance_mm) {
    return request.closed_loop_anchor_tolerance_mm > 0.0f
        ? request.closed_loop_anchor_tolerance_mm
        : vertex_tolerance_mm;
}

bool PointsNear(const Siligen::Shared::Types::Point2D& lhs,
                const Siligen::Shared::Types::Point2D& rhs,
                float32 tolerance_mm) {
    return lhs.DistanceTo(rhs) <= tolerance_mm;
}

Result<float32> MeasureSegmentLength(
    const Segment& segment,
    const AuthorityTriggerLayoutPlannerRequest& request) {
    switch (segment.type) {
        case SegmentType::Line: {
            const float32 length_mm = segment.line.start.DistanceTo(segment.line.end);
            if (!std::isfinite(length_mm)) {
                return Result<float32>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "line contains non-finite coordinates", "AuthorityTriggerLayoutPlanner"));
            }
            return Result<float32>::Success(length_mm);
        }
        case SegmentType::Arc: {
            const float32 length_mm = Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength(segment.arc);
            if (!std::isfinite(length_mm)) {
                return Result<float32>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "arc geometry is invalid", "AuthorityTriggerLayoutPlanner"));
            }
            return Result<float32>::Success(length_mm);
        }
        case SegmentType::Spline: {
            CurveFlatteningService flattening_service;
            auto flattened_result = flattening_service.Flatten(
                segment,
                request.spline_max_error_mm,
                request.spline_max_step_mm);
            if (flattened_result.IsError()) {
                return Result<float32>::Failure(flattened_result.GetError());
            }
            return Result<float32>::Success(flattened_result.Value().total_length_mm);
        }
        default:
            return Result<float32>::Failure(
                Error(ErrorCode::NOT_IMPLEMENTED, "unsupported segment type", "AuthorityTriggerLayoutPlanner"));
    }
}

bool IsClosedSpan(const TopologySpanSlice& span, float32 vertex_tolerance_mm) {
    if (span.segments.empty()) {
        return false;
    }
    if (span.segments.size() == 1U &&
        span.segments.front().geometry.type == SegmentType::Spline &&
        span.segments.front().geometry.spline.closed) {
        return true;
    }
    return SegmentStart(span.segments.front().geometry).DistanceTo(
               SegmentEnd(span.segments.back().geometry)) <= vertex_tolerance_mm;
}

DispenseSpanCurveMode ResolveCurveMode(const TopologySpanSlice& span) {
    bool has_line = false;
    bool has_arc = false;
    bool has_spline = false;
    for (const auto& process_segment : span.segments) {
        switch (process_segment.geometry.type) {
            case SegmentType::Line:
                has_line = true;
                break;
            case SegmentType::Arc:
                has_arc = true;
                break;
            case SegmentType::Spline:
                has_spline = true;
                break;
            default:
                break;
        }
    }
    if (has_spline) {
        return DispenseSpanCurveMode::FlattenedCurve;
    }
    if (has_line && !has_arc) {
        return DispenseSpanCurveMode::Line;
    }
    if (has_arc && !has_line) {
        return DispenseSpanCurveMode::Arc;
    }
    return DispenseSpanCurveMode::Mixed;
}

float32 ComputeSpanLength(const TopologySpanSlice& span) {
    float32 total_length_mm = 0.0f;
    for (float32 segment_length_mm : span.segment_lengths_mm) {
        total_length_mm += segment_length_mm;
    }
    return total_length_mm;
}

std::string BuildLayoutSeed(const AuthorityTriggerLayoutPlannerRequest& request,
                            const TopologySpanSplitterResult& topology_result,
                            const TopologyComponentClassifierResult& component_result) {
    std::ostringstream oss;
    oss << request.layout_id_seed
        << '|'
        << topology_result.spans.size()
        << '|'
        << request.target_spacing_mm
        << '|'
        << (topology_result.branch_revisit_split_applied ? 1 : 0)
        << '|'
        << (request.enable_closed_loop_corner_anchors ? 1 : 0)
        << '|'
        << request.closed_loop_corner_angle_threshold_deg
        << '|'
        << request.closed_loop_corner_cluster_distance_mm
        << '|'
        << request.closed_loop_anchor_tolerance_mm
        << '|'
        << ToString(component_result.dispatch_type)
        << '|'
        << component_result.effective_component_count
        << '|'
        << component_result.ignored_component_count;
    for (const auto& span : topology_result.spans) {
        oss << '|'
            << span.start_distance_mm
            << ':'
            << ToString(span.split_reason)
            << ':'
            << span.source_segment_indices.size();
        for (const auto source_segment_index : span.source_segment_indices) {
            oss << ',' << source_segment_index;
        }
    }
    for (const auto& component : component_result.components) {
        oss << '|'
            << ToString(component.dispatch_type)
            << ':'
            << (component.ignored ? 1 : 0)
            << ':'
            << component.ignored_reason
            << ':'
            << component.blocking_reason
            << ':'
            << component.spans.size();
        for (const auto source_segment_index : component.source_segment_indices) {
            oss << ',' << source_segment_index;
        }
        for (const auto& span : component.spans) {
            oss << ';'
                << span.start_distance_mm
                << ':'
                << ToString(span.split_reason)
                << ':'
                << span.source_segment_indices.size();
        }
    }
    return oss.str();
}

std::string BuildComponentSeed(const std::string& layout_id,
                               const TopologyComponent& component,
                               std::size_t component_index) {
    std::ostringstream oss;
    oss << layout_id
        << '|'
        << component_index
        << '|'
        << ToString(component.dispatch_type)
        << '|'
        << (component.ignored ? 1 : 0)
        << '|'
        << component.ignored_reason
        << '|'
        << component.blocking_reason
        << '|'
        << component.spans.size();
    for (const auto source_segment_index : component.source_segment_indices) {
        oss << ',' << source_segment_index;
    }
    return oss.str();
}

void AppendStrongAnchor(DispenseSpan& layout_span,
                        const std::string& span_id,
                        const Siligen::Shared::Types::Point2D& position,
                        float32 distance_mm_span,
                        std::size_t source_segment_index,
                        StrongAnchorRole role,
                        float32 tolerance_mm,
                        float32 significance_angle_deg = 0.0f) {
    for (const auto& existing : layout_span.strong_anchors) {
        if (existing.role == role &&
            existing.source_segment_index == source_segment_index &&
            PointsNear(existing.position, position, tolerance_mm)) {
            return;
        }
    }

    StrongAnchor anchor;
    anchor.span_ref = span_id;
    anchor.position = position;
    anchor.distance_mm_span = distance_mm_span;
    anchor.significance_angle_deg = significance_angle_deg;
    anchor.source_segment_index = source_segment_index;
    anchor.role = role;
    std::ostringstream seed;
    seed << span_id << '|'
         << ToString(role) << '|'
         << source_segment_index << '|'
         << position.x << '|'
         << position.y << '|'
         << distance_mm_span;
    anchor.anchor_id = BuildStableId("anchor", seed.str());
    layout_span.strong_anchors.push_back(anchor);
}

bool HasExplicitStrongAnchor(const DispenseSpan& layout_span) {
    return std::any_of(
        layout_span.strong_anchors.begin(),
        layout_span.strong_anchors.end(),
        [](const auto& anchor) {
            return anchor.role == StrongAnchorRole::SplitBoundary ||
                   anchor.role == StrongAnchorRole::ExceptionBoundary;
        });
}

bool HasStrongAnchorAtPosition(const DispenseSpan& layout_span,
                               const Siligen::Shared::Types::Point2D& position,
                               float32 tolerance_mm) {
    return std::any_of(
        layout_span.strong_anchors.begin(),
        layout_span.strong_anchors.end(),
        [&](const auto& anchor) { return PointsNear(anchor.position, position, tolerance_mm); });
}

void PopulateStrongAnchors(DispenseSpan& layout_span,
                           const TopologySpanSlice& span,
                           float32 total_length_mm,
                           float32 tolerance_mm) {
    if (span.segments.empty()) {
        return;
    }

    if (!layout_span.closed) {
        AppendStrongAnchor(
            layout_span,
            layout_span.span_id,
            SegmentStart(span.segments.front().geometry),
            0.0f,
            span.source_segment_indices.front(),
            StrongAnchorRole::OpenStart,
            tolerance_mm);
        AppendStrongAnchor(
            layout_span,
            layout_span.span_id,
            SegmentEnd(span.segments.back().geometry),
            total_length_mm,
            span.source_segment_indices.back(),
            StrongAnchorRole::OpenEnd,
            tolerance_mm);
    }

    for (const auto& candidate : span.strong_anchor_candidates) {
        AppendStrongAnchor(
            layout_span,
            layout_span.span_id,
            candidate.position,
            candidate.distance_mm_span,
            candidate.source_segment_index,
            candidate.role,
            tolerance_mm);
    }
}

void ResolveSpanPolicies(DispenseSpan& layout_span) {
    layout_span.topology_type = layout_span.closed
        ? DispenseSpanTopologyType::ClosedLoop
        : DispenseSpanTopologyType::OpenChain;
    if (layout_span.closed) {
        if (layout_span.strong_anchors.empty()) {
            layout_span.anchor_policy = DispenseSpanAnchorPolicy::ClosedLoopPhaseSearch;
            layout_span.phase_strategy = DispenseSpanPhaseStrategy::DeterministicSearch;
        } else {
            layout_span.anchor_policy = DispenseSpanAnchorPolicy::ClosedLoopAnchorConstrained;
            layout_span.phase_strategy = DispenseSpanPhaseStrategy::AnchorConstrained;
        }
        return;
    }

    layout_span.phase_strategy = DispenseSpanPhaseStrategy::FixedZero;
    layout_span.anchor_policy = HasExplicitStrongAnchor(layout_span)
        ? DispenseSpanAnchorPolicy::PreserveStrongAnchors
        : DispenseSpanAnchorPolicy::PreserveEndpoints;
}

SpacingValidationClassification ResolveClassification(
    float32 actual_spacing_mm,
    float32 min_spacing_mm,
    float32 max_spacing_mm,
    std::string& exception_reason,
    std::string& blocking_reason) {
    exception_reason.clear();
    blocking_reason.clear();
    if (!std::isfinite(actual_spacing_mm) || actual_spacing_mm <= kEpsilon) {
        blocking_reason = "authority spacing unavailable";
        return SpacingValidationClassification::Fail;
    }
    if (actual_spacing_mm >= min_spacing_mm - kEpsilon && actual_spacing_mm <= max_spacing_mm + kEpsilon) {
        return SpacingValidationClassification::Pass;
    }
    exception_reason = "span spacing outside configured window but accepted as explicit exception";
    return SpacingValidationClassification::PassWithException;
}

std::vector<float32> BuildSegmentBoundaries(const TopologySpanSlice& span) {
    std::vector<float32> boundaries;
    float32 accumulated_mm = 0.0f;
    for (std::size_t index = 0; index < span.segment_lengths_mm.size(); ++index) {
        accumulated_mm += span.segment_lengths_mm[index];
        if (index + 1 < span.segment_lengths_mm.size()) {
            boundaries.push_back(accumulated_mm);
        }
    }
    return boundaries;
}

bool IsSharedVertexDistance(const std::vector<float32>& boundaries, float32 distance_mm) {
    for (float32 boundary : boundaries) {
        if (std::fabs(boundary - distance_mm) <= kSharedVertexToleranceMm) {
            return true;
        }
    }
    return false;
}

void AppendClosedLoopCornerAnchors(
    DispenseSpan& layout_span,
    const Internal::ClosedLoopCornerResolution& resolution,
    float32 tolerance_mm) {
    layout_span.candidate_corner_count = resolution.candidate_corner_count;
    layout_span.accepted_corner_count = resolution.accepted_candidates.size();
    layout_span.suppressed_corner_count = resolution.suppressed_corner_count;
    layout_span.dense_corner_cluster_count = resolution.dense_corner_cluster_count;
    for (const auto& candidate : resolution.accepted_candidates) {
        AppendStrongAnchor(
            layout_span,
            layout_span.span_id,
            candidate.position,
            candidate.distance_mm_span,
            candidate.source_segment_index,
            StrongAnchorRole::ClosedLoopCorner,
            tolerance_mm,
            candidate.significance_angle_deg);
    }
}

bool HasStrongAnchorAtDistance(const DispenseSpan& layout_span,
                               float32 distance_mm_span,
                               float32 total_length_mm,
                               float32 tolerance_mm) {
    return std::any_of(
        layout_span.strong_anchors.begin(),
        layout_span.strong_anchors.end(),
        [&](const auto& anchor) {
            return Internal::CircularDistance(
                       Internal::WrapLoopDistance(anchor.distance_mm_span, total_length_mm),
                       distance_mm_span,
                       total_length_mm) <= tolerance_mm + kEpsilon;
        });
}

SpacingValidationOutcome BuildBlockingOutcome(
    const std::string& layout_id,
    const std::string& span_id,
    const std::string& blocking_reason) {
    SpacingValidationOutcome outcome;
    outcome.outcome_id = BuildStableId("spacing", span_id);
    outcome.layout_ref = layout_id;
    outcome.span_ref = span_id;
    outcome.classification = SpacingValidationClassification::Fail;
    outcome.anchor_constraint_state = "not_applicable";
    outcome.blocking_reason = blocking_reason;
    return outcome;
}

void PopulateOutcomeDiagnostics(SpacingValidationOutcome& outcome, const DispenseSpan& layout_span) {
    outcome.component_id = layout_span.component_id;
    outcome.component_index = layout_span.component_index;
    outcome.topology_type = layout_span.topology_type;
    outcome.dispatch_type = layout_span.dispatch_type;
    outcome.split_reason = layout_span.split_reason;
    outcome.anchor_policy = layout_span.anchor_policy;
    outcome.phase_strategy = layout_span.phase_strategy;
}

}  // namespace

Result<AuthorityTriggerLayout> AuthorityTriggerLayoutPlanner::Plan(
    const AuthorityTriggerLayoutPlannerRequest& request) const {
    if (request.process_path.segments.empty()) {
        return Result<AuthorityTriggerLayout>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "process path is empty", "AuthorityTriggerLayoutPlanner"));
    }
    if (request.target_spacing_mm <= kEpsilon) {
        return Result<AuthorityTriggerLayout>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "target spacing is invalid", "AuthorityTriggerLayoutPlanner"));
    }

    std::vector<float32> segment_lengths_mm;
    segment_lengths_mm.reserve(request.process_path.segments.size());
    for (const auto& process_segment : request.process_path.segments) {
        if (process_segment.dispense_on && process_segment.geometry.is_point) {
            return Result<AuthorityTriggerLayout>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "dispense_on 几何线段长度为0",
                      "AuthorityTriggerLayoutPlanner"));
        }
        if (process_segment.dispense_on &&
            process_segment.geometry.type != SegmentType::Spline &&
            SegmentStart(process_segment.geometry).DistanceTo(SegmentEnd(process_segment.geometry)) <= kEpsilon) {
            return Result<AuthorityTriggerLayout>::Failure(
                Error(ErrorCode::INVALID_PARAMETER,
                      "dispense_on 几何线段长度为0",
                      "AuthorityTriggerLayoutPlanner"));
        }

        const auto length_result = MeasureSegmentLength(process_segment.geometry, request);
        segment_lengths_mm.push_back(length_result.IsSuccess() ? length_result.Value() : 0.0f);
    }

    AuthorityTriggerLayout layout;
    layout.plan_id = request.plan_id;
    layout.plan_fingerprint = request.plan_fingerprint;
    layout.target_spacing_mm = request.target_spacing_mm;
    layout.min_spacing_mm = ResolveSpacingMin(request);
    layout.max_spacing_mm = ResolveSpacingMax(request);
    const float32 vertex_tolerance_mm = ResolveVertexTolerance(request);
    const float32 closed_loop_corner_cluster_distance_mm =
        ResolveClosedLoopCornerClusterDistance(request, layout.min_spacing_mm);
    const float32 closed_loop_anchor_tolerance_mm =
        ResolveClosedLoopAnchorTolerance(request, vertex_tolerance_mm);

    TopologySpanSplitter splitter;
    TopologySpanSplitterRequest split_request;
    split_request.process_path = request.process_path;
    split_request.segment_lengths_mm = segment_lengths_mm;
    split_request.vertex_tolerance_mm = vertex_tolerance_mm;
    split_request.enable_branch_revisit_split = request.enable_branch_revisit_split;

    const auto topology_result = splitter.Split(split_request);
    TopologyComponentClassifier classifier;
    const auto component_result = classifier.Classify({
        topology_result.spans,
        vertex_tolerance_mm,
        layout.min_spacing_mm,
    });

    layout.topology_dispatch_applied = topology_result.topology_dispatch_applied ||
        component_result.multi_contour_split_applied ||
        component_result.auxiliary_geometry_ignored;
    layout.branch_revisit_split_applied = topology_result.branch_revisit_split_applied;
    layout.dispatch_type = component_result.dispatch_type;
    layout.effective_component_count = component_result.effective_component_count;
    layout.ignored_component_count = component_result.ignored_component_count;

    if (topology_result.spans.empty()) {
        layout.layout_id = BuildStableId("layout", request.layout_id_seed + "|empty");
        layout.dispatch_type = TopologyDispatchType::UnsupportedMixedTopology;
        layout.state = AuthorityTriggerLayoutState::Blocked;
        return Result<AuthorityTriggerLayout>::Success(std::move(layout));
    }

    layout.layout_id = BuildStableId("layout", BuildLayoutSeed(request, topology_result, component_result));

    TriggerPlan trigger_plan;
    trigger_plan.strategy = request.dispensing_strategy;
    trigger_plan.interval_mm = request.target_spacing_mm;
    trigger_plan.subsegment_count = request.subsegment_count;
    trigger_plan.dispense_only_cruise = request.dispense_only_cruise;
    trigger_plan.safety.duration_ms = static_cast<int32>(request.dispenser_duration_ms);
    trigger_plan.safety.valve_response_ms = static_cast<int32>(request.valve_response_ms);
    trigger_plan.safety.margin_ms = static_cast<int32>(request.safety_margin_ms);
    trigger_plan.safety.min_interval_ms = static_cast<int32>(request.min_interval_ms);
    trigger_plan.safety.downgrade_on_violation = request.downgrade_on_violation;

    TriggerPlanner trigger_planner;
    PathArcLengthLocator locator;
    Internal::ClosedLoopCornerAnchorResolver corner_anchor_resolver;
    Internal::ClosedLoopAnchorConstraintSolver closed_loop_solver;

    std::size_t global_sequence = 0;
    bool has_blocking_failure = component_result.effective_component_count == 0U;
    std::size_t span_order_index = 0;
    layout.components.reserve(component_result.components.size());

    for (std::size_t component_index = 0; component_index < component_result.components.size(); ++component_index) {
        const auto& component = component_result.components[component_index];

        AuthorityLayoutComponent component_record;
        component_record.component_id = BuildStableId(
            "component",
            BuildComponentSeed(layout.layout_id, component, component_index));
        component_record.layout_ref = layout.layout_id;
        component_record.component_index = component_index;
        component_record.dispatch_type = component.dispatch_type;
        component_record.ignored = component.ignored;
        component_record.ignored_reason = component.ignored_reason;
        component_record.blocking_reason = component.blocking_reason;
        component_record.source_segment_indices = component.source_segment_indices;

        if (component.ignored) {
            layout.components.push_back(std::move(component_record));
            continue;
        }

        for (std::size_t local_span_index = 0; local_span_index < component.spans.size(); ++local_span_index) {
            const auto& span = component.spans[local_span_index];
            const std::string span_id = BuildStableId(
                "span",
                component_record.component_id + "|" + std::to_string(local_span_index));
            component_record.span_refs.push_back(span_id);

            DispenseSpan layout_span;
            layout_span.span_id = span_id;
            layout_span.layout_ref = layout.layout_id;
            layout_span.component_id = component_record.component_id;
            layout_span.component_index = component_record.component_index;
            layout_span.source_segment_indices = span.source_segment_indices;
            layout_span.order_index = span_order_index++;
            layout_span.closed = IsClosedSpan(span, split_request.vertex_tolerance_mm);
            layout_span.curve_mode = ResolveCurveMode(span);
            layout_span.dispatch_type = component.dispatch_type;
            layout_span.split_reason = span.split_reason;

            auto append_failed_span =
                [&](const std::string& reason, const std::string& anchor_constraint_state = "not_applicable") {
                    layout_span.validation_state = SpacingValidationClassification::Fail;
                    has_blocking_failure = true;
                    auto outcome = BuildBlockingOutcome(layout.layout_id, span_id, reason);
                    outcome.anchor_constraint_state = anchor_constraint_state;
                    PopulateOutcomeDiagnostics(outcome, layout_span);
                    layout.spans.push_back(layout_span);
                    layout.validation_outcomes.push_back(std::move(outcome));
                };

            if (component.dispatch_type == TopologyDispatchType::UnsupportedMixedTopology) {
                append_failed_span(component_record.blocking_reason);
                continue;
            }

            const float32 total_length_mm = ComputeSpanLength(span);
            if (total_length_mm <= kEpsilon) {
                append_failed_span("authority span length unavailable");
                continue;
            }

            PopulateStrongAnchors(layout_span, span, total_length_mm, vertex_tolerance_mm);
            if (layout_span.closed) {
                const auto corner_resolution = corner_anchor_resolver.Resolve({
                    &span,
                    &layout_span,
                    total_length_mm,
                    request.closed_loop_corner_angle_threshold_deg,
                    closed_loop_corner_cluster_distance_mm,
                    closed_loop_anchor_tolerance_mm,
                    vertex_tolerance_mm,
                    request.enable_closed_loop_corner_anchors,
                });
                AppendClosedLoopCornerAnchors(layout_span, corner_resolution, vertex_tolerance_mm);
            }
            ResolveSpanPolicies(layout_span);

            auto timing_result = trigger_planner.Plan(
                total_length_mm,
                request.dispensing_velocity,
                request.acceleration,
                request.target_spacing_mm,
                request.dispenser_interval_ms,
                0.0f,
                trigger_plan,
                request.compensation_profile);
            if (timing_result.IsError()) {
                return Result<AuthorityTriggerLayout>::Failure(timing_result.GetError());
            }

            const std::size_t default_division_count = std::max<std::size_t>(
                1,
                static_cast<std::size_t>(std::lround(total_length_mm / request.target_spacing_mm)));
            const auto boundaries = BuildSegmentBoundaries(span);

            layout_span.total_length_mm = total_length_mm;
            layout_span.interval_count = default_division_count;
            layout_span.actual_spacing_mm = total_length_mm / static_cast<float32>(default_division_count);
            layout_span.phase_mm = 0.0f;
            layout_span.anchor_constraints_satisfied = false;
            if (layout_span.closed) {
                if (layout_span.phase_strategy == DispenseSpanPhaseStrategy::AnchorConstrained) {
                    const auto anchor_solution = closed_loop_solver.Solve({
                        &layout_span.strong_anchors,
                        total_length_mm,
                        request.target_spacing_mm,
                        layout.min_spacing_mm,
                        layout.max_spacing_mm,
                        closed_loop_anchor_tolerance_mm,
                    });
                    if (!anchor_solution.solved) {
                        append_failed_span(anchor_solution.blocking_reason, anchor_solution.anchor_constraint_state);
                        continue;
                    }
                    layout_span.interval_count = anchor_solution.interval_count;
                    layout_span.actual_spacing_mm = anchor_solution.actual_spacing_mm;
                    layout_span.phase_mm = anchor_solution.phase_mm;
                    layout_span.anchor_constraints_satisfied = true;
                } else {
                    layout_span.phase_mm = closed_loop_solver.ResolvePhase(
                        total_length_mm,
                        layout_span.actual_spacing_mm,
                        layout_span.interval_count,
                        boundaries);
                }
            }

            SpacingValidationOutcome outcome;
            outcome.outcome_id = BuildStableId("spacing", layout_span.span_id);
            outcome.layout_ref = layout.layout_id;
            outcome.span_ref = layout_span.span_id;
            if (layout_span.closed && layout_span.phase_strategy == DispenseSpanPhaseStrategy::AnchorConstrained) {
                if (layout_span.actual_spacing_mm >= layout.min_spacing_mm - kEpsilon &&
                    layout_span.actual_spacing_mm <= layout.max_spacing_mm + kEpsilon) {
                    outcome.classification = SpacingValidationClassification::Pass;
                    outcome.anchor_constraint_state = "satisfied";
                } else {
                    outcome.classification = SpacingValidationClassification::PassWithException;
                    outcome.anchor_constraint_state = "satisfied_with_exception";
                    outcome.anchor_exception_reason = "anchor_constrained_spacing_outside_window";
                    outcome.exception_reason = outcome.anchor_exception_reason;
                }
            } else {
                outcome.classification = ResolveClassification(
                    layout_span.actual_spacing_mm,
                    layout.min_spacing_mm,
                    layout.max_spacing_mm,
                    outcome.exception_reason,
                    outcome.blocking_reason);
                outcome.anchor_constraint_state = "not_applicable";
            }
            outcome.min_observed_spacing_mm = layout_span.actual_spacing_mm;
            outcome.max_observed_spacing_mm = layout_span.actual_spacing_mm;
            PopulateOutcomeDiagnostics(outcome, layout_span);
            layout_span.validation_state = outcome.classification;
            if (outcome.classification == SpacingValidationClassification::Fail) {
                has_blocking_failure = true;
                layout.spans.push_back(layout_span);
                layout.validation_outcomes.push_back(std::move(outcome));
                continue;
            }

            const std::size_t point_count =
                layout_span.closed ? layout_span.interval_count : (layout_span.interval_count + 1U);
            std::vector<LayoutTriggerPoint> span_trigger_points;
            span_trigger_points.reserve(point_count);
            bool span_failed = false;
            for (std::size_t point_index = 0; point_index < point_count; ++point_index) {
                const float32 distance_mm = layout_span.closed
                    ? Internal::WrapLoopDistance(
                        layout_span.phase_mm + layout_span.actual_spacing_mm * static_cast<float32>(point_index),
                        total_length_mm)
                    : ((point_index == layout_span.interval_count)
                        ? total_length_mm
                        : layout_span.actual_spacing_mm * static_cast<float32>(point_index));
                auto location_result = locator.Locate(
                    span.segments,
                    distance_mm,
                    request.spline_max_error_mm,
                    request.spline_max_step_mm);
                if (location_result.IsError()) {
                    outcome.classification = SpacingValidationClassification::Fail;
                    outcome.exception_reason.clear();
                    outcome.blocking_reason = location_result.GetError().GetMessage();
                    layout_span.validation_state = outcome.classification;
                    has_blocking_failure = true;
                    span_failed = true;
                    break;
                }

                LayoutTriggerPoint point;
                point.trigger_id = BuildStableId(
                    "trigger",
                    layout_span.span_id + "|" + std::to_string(point_index) + "|" + std::to_string(distance_mm));
                point.layout_ref = layout.layout_id;
                point.span_ref = layout_span.span_id;
                point.sequence_index_span = point_index;
                point.distance_mm_global = span.start_distance_mm + distance_mm;
                point.distance_mm_span = distance_mm;
                point.position = location_result.Value().position;
                point.source_segment_index = span.source_segment_indices[location_result.Value().segment_index];
                point.shared_vertex = IsSharedVertexDistance(boundaries, distance_mm);
                if ((!layout_span.closed && (point_index == 0 || point_index + 1 == point_count)) ||
                    (layout_span.closed &&
                     HasStrongAnchorAtDistance(
                         layout_span,
                         distance_mm,
                         total_length_mm,
                         closed_loop_anchor_tolerance_mm)) ||
                    HasStrongAnchorAtPosition(layout_span, point.position, split_request.vertex_tolerance_mm)) {
                    point.source_kind = LayoutTriggerSourceKind::Anchor;
                } else if (point.shared_vertex) {
                    point.source_kind = LayoutTriggerSourceKind::SharedVertex;
                } else {
                    point.source_kind = LayoutTriggerSourceKind::Generated;
                }

                span_trigger_points.push_back(point);
                outcome.trigger_refs.push_back(point.trigger_id);
                outcome.points.push_back(point.position);
            }

            if (span_failed) {
                layout.spans.push_back(layout_span);
                layout.validation_outcomes.push_back(std::move(outcome));
                continue;
            }
            if (span_trigger_points.empty()) {
                outcome.classification = SpacingValidationClassification::Fail;
                outcome.exception_reason.clear();
                outcome.blocking_reason = "authority trigger points unavailable";
                layout_span.validation_state = outcome.classification;
                has_blocking_failure = true;
                layout.spans.push_back(layout_span);
                layout.validation_outcomes.push_back(std::move(outcome));
                continue;
            }

            for (auto& point : span_trigger_points) {
                point.sequence_index_global = global_sequence++;
                layout.trigger_points.push_back(std::move(point));
            }

            layout.spans.push_back(std::move(layout_span));
            layout.validation_outcomes.push_back(std::move(outcome));
        }

        layout.components.push_back(std::move(component_record));
    }

    layout.authority_ready =
        layout.effective_component_count > 0U &&
        !layout.trigger_points.empty() &&
        !has_blocking_failure;
    layout.state = layout.authority_ready
        ? AuthorityTriggerLayoutState::LayoutReady
        : AuthorityTriggerLayoutState::Blocked;
    return Result<AuthorityTriggerLayout>::Success(std::move(layout));
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
