#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::Domain::Dispensing::ValueObjects {

using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::float32;

enum class AuthorityTriggerLayoutState {
    LayoutReady,
    BindingReady,
    SnapshotReady,
    Confirmed,
    Blocked,
    Stale,
};

enum class DispenseSpanCurveMode {
    Line,
    Arc,
    Mixed,
    FlattenedCurve,
};

enum class DispenseSpanTopologyType {
    OpenChain,
    ClosedLoop,
};

enum class TopologyDispatchType {
    SingleOpenChain,
    SingleClosedLoop,
    BranchOrRevisit,
    MultiContour,
    ExplicitProcessBoundary,
    AuxiliaryGeometry,
    UnsupportedMixedTopology,
};

enum class DispenseSpanSplitReason {
    None,
    BranchOrRevisit,
    MultiContourBoundary,
    ExplicitProcessBoundary,
    FormalCompareFeasibility,
    ExceptionFeature,
};

enum class DispenseSpanAnchorPolicy {
    PreserveEndpoints,
    PreserveStrongAnchors,
    ClosedLoopPhaseSearch,
    ClosedLoopAnchorConstrained,
};

enum class DispenseSpanPhaseStrategy {
    FixedZero,
    CarryForward,
    DeterministicSearch,
    AnchorConstrained,
};

enum class SpacingValidationClassification {
    Pass,
    PassWithException,
    Fail,
};

enum class LayoutTriggerSourceKind {
    Anchor,
    Generated,
    SharedVertex,
};

enum class StrongAnchorRole {
    OpenStart,
    OpenEnd,
    SplitBoundary,
    ExceptionBoundary,
    ClosedLoopCorner,
};

struct StrongAnchor {
    std::string anchor_id;
    std::string span_ref;
    Point2D position;
    float32 distance_mm_span = 0.0f;
    float32 significance_angle_deg = 0.0f;
    std::size_t source_segment_index = 0;
    StrongAnchorRole role = StrongAnchorRole::SplitBoundary;
    bool required = true;
};

struct LayoutTriggerPoint {
    std::string trigger_id;
    std::string layout_ref;
    std::string span_ref;
    std::size_t sequence_index_global = 0;
    std::size_t sequence_index_span = 0;
    float32 distance_mm_global = 0.0f;
    float32 distance_mm_span = 0.0f;
    Point2D position;
    LayoutTriggerSourceKind source_kind = LayoutTriggerSourceKind::Generated;
    bool preview_visible = true;
    std::size_t source_segment_index = 0;
    bool shared_vertex = false;
};

struct InterpolationTriggerBinding {
    std::string binding_id;
    std::string layout_ref;
    std::string trigger_ref;
    std::size_t interpolation_index = 0;
    Point2D execution_position;
    float32 match_error_mm = 0.0f;
    bool monotonic = true;
    bool bound = false;
};

struct SpacingValidationOutcome {
    std::string outcome_id;
    std::string layout_ref;
    std::string span_ref;
    std::string component_id;
    std::size_t component_index = 0;
    std::vector<std::string> trigger_refs;
    std::vector<Point2D> points;
    DispenseSpanTopologyType topology_type = DispenseSpanTopologyType::OpenChain;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    DispenseSpanSplitReason split_reason = DispenseSpanSplitReason::None;
    DispenseSpanAnchorPolicy anchor_policy = DispenseSpanAnchorPolicy::PreserveEndpoints;
    DispenseSpanPhaseStrategy phase_strategy = DispenseSpanPhaseStrategy::FixedZero;
    SpacingValidationClassification classification = SpacingValidationClassification::Pass;
    float32 min_observed_spacing_mm = 0.0f;
    float32 max_observed_spacing_mm = 0.0f;
    std::string anchor_constraint_state;
    std::string exception_reason;
    std::string anchor_exception_reason;
    std::string blocking_reason;
};

struct DispenseSpan {
    std::string span_id;
    std::string layout_ref;
    std::string component_id;
    std::size_t component_index = 0;
    std::vector<std::size_t> source_segment_indices;
    std::size_t order_index = 0;
    bool reverse_traversal = false;
    bool closed = false;
    DispenseSpanTopologyType topology_type = DispenseSpanTopologyType::OpenChain;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    DispenseSpanSplitReason split_reason = DispenseSpanSplitReason::None;
    DispenseSpanAnchorPolicy anchor_policy = DispenseSpanAnchorPolicy::PreserveEndpoints;
    DispenseSpanPhaseStrategy phase_strategy = DispenseSpanPhaseStrategy::FixedZero;
    float32 total_length_mm = 0.0f;
    float32 actual_spacing_mm = 0.0f;
    std::size_t interval_count = 0;
    float32 phase_mm = 0.0f;
    DispenseSpanCurveMode curve_mode = DispenseSpanCurveMode::Line;
    SpacingValidationClassification validation_state = SpacingValidationClassification::Pass;
    std::size_t candidate_corner_count = 0;
    std::size_t accepted_corner_count = 0;
    std::size_t suppressed_corner_count = 0;
    std::size_t dense_corner_cluster_count = 0;
    bool anchor_constraints_satisfied = false;
    std::vector<StrongAnchor> strong_anchors;
};

struct AuthorityLayoutComponent {
    std::string component_id;
    std::string layout_ref;
    std::size_t component_index = 0;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    bool ignored = false;
    std::string ignored_reason;
    std::string blocking_reason;
    std::vector<std::size_t> source_segment_indices;
    std::vector<std::string> span_refs;
};

struct AuthorityTriggerLayout {
    std::string layout_id;
    std::string plan_id;
    std::string plan_fingerprint;
    std::string origin = "continuous-span-layout";
    std::string preview_source = "planned_glue_snapshot";
    std::string preview_kind = "glue_points";
    float32 target_spacing_mm = 0.0f;
    float32 min_spacing_mm = 0.0f;
    float32 max_spacing_mm = 0.0f;
    AuthorityTriggerLayoutState state = AuthorityTriggerLayoutState::Blocked;
    bool authority_ready = false;
    bool binding_ready = false;
    bool topology_dispatch_applied = false;
    bool branch_revisit_split_applied = false;
    TopologyDispatchType dispatch_type = TopologyDispatchType::SingleOpenChain;
    std::size_t effective_component_count = 0;
    std::size_t ignored_component_count = 0;
    std::vector<AuthorityLayoutComponent> components;
    std::vector<DispenseSpan> spans;
    std::vector<LayoutTriggerPoint> trigger_points;
    std::vector<InterpolationTriggerBinding> bindings;
    std::vector<SpacingValidationOutcome> validation_outcomes;
};

inline const char* ToString(AuthorityTriggerLayoutState state) {
    switch (state) {
        case AuthorityTriggerLayoutState::LayoutReady:
            return "layout_ready";
        case AuthorityTriggerLayoutState::BindingReady:
            return "binding_ready";
        case AuthorityTriggerLayoutState::SnapshotReady:
            return "snapshot_ready";
        case AuthorityTriggerLayoutState::Confirmed:
            return "confirmed";
        case AuthorityTriggerLayoutState::Blocked:
            return "blocked";
        case AuthorityTriggerLayoutState::Stale:
            return "stale";
    }
    return "blocked";
}

inline const char* ToString(SpacingValidationClassification classification) {
    switch (classification) {
        case SpacingValidationClassification::Pass:
            return "pass";
        case SpacingValidationClassification::PassWithException:
            return "pass_with_exception";
        case SpacingValidationClassification::Fail:
            return "fail";
    }
    return "fail";
}

inline const char* ToString(DispenseSpanTopologyType topology_type) {
    switch (topology_type) {
        case DispenseSpanTopologyType::OpenChain:
            return "open_chain";
        case DispenseSpanTopologyType::ClosedLoop:
            return "closed_loop";
    }
    return "open_chain";
}

inline const char* ToString(TopologyDispatchType dispatch_type) {
    switch (dispatch_type) {
        case TopologyDispatchType::SingleOpenChain:
            return "single_open_chain";
        case TopologyDispatchType::SingleClosedLoop:
            return "single_closed_loop";
        case TopologyDispatchType::BranchOrRevisit:
            return "branch_or_revisit";
        case TopologyDispatchType::MultiContour:
            return "multi_contour";
        case TopologyDispatchType::ExplicitProcessBoundary:
            return "explicit_process_boundary";
        case TopologyDispatchType::AuxiliaryGeometry:
            return "auxiliary_geometry";
        case TopologyDispatchType::UnsupportedMixedTopology:
            return "unsupported_mixed_topology";
    }
    return "unsupported_mixed_topology";
}

inline const char* ToString(DispenseSpanSplitReason split_reason) {
    switch (split_reason) {
        case DispenseSpanSplitReason::None:
            return "none";
        case DispenseSpanSplitReason::BranchOrRevisit:
            return "branch_or_revisit";
        case DispenseSpanSplitReason::MultiContourBoundary:
            return "multi_contour_boundary";
        case DispenseSpanSplitReason::ExplicitProcessBoundary:
            return "explicit_process_boundary";
        case DispenseSpanSplitReason::FormalCompareFeasibility:
            return "formal_compare_feasibility";
        case DispenseSpanSplitReason::ExceptionFeature:
            return "exception_feature";
    }
    return "none";
}

inline const char* ToString(DispenseSpanAnchorPolicy anchor_policy) {
    switch (anchor_policy) {
        case DispenseSpanAnchorPolicy::PreserveEndpoints:
            return "preserve_endpoints";
        case DispenseSpanAnchorPolicy::PreserveStrongAnchors:
            return "preserve_strong_anchors";
        case DispenseSpanAnchorPolicy::ClosedLoopPhaseSearch:
            return "closed_loop_phase_search";
        case DispenseSpanAnchorPolicy::ClosedLoopAnchorConstrained:
            return "closed_loop_anchor_constrained";
    }
    return "preserve_endpoints";
}

inline const char* ToString(DispenseSpanPhaseStrategy phase_strategy) {
    switch (phase_strategy) {
        case DispenseSpanPhaseStrategy::FixedZero:
            return "fixed_zero";
        case DispenseSpanPhaseStrategy::CarryForward:
            return "carry_forward";
        case DispenseSpanPhaseStrategy::DeterministicSearch:
            return "deterministic_search";
        case DispenseSpanPhaseStrategy::AnchorConstrained:
            return "anchor_constrained";
    }
    return "fixed_zero";
}

inline const char* ToString(StrongAnchorRole role) {
    switch (role) {
        case StrongAnchorRole::OpenStart:
            return "open_start";
        case StrongAnchorRole::OpenEnd:
            return "open_end";
        case StrongAnchorRole::SplitBoundary:
            return "split_boundary";
        case StrongAnchorRole::ExceptionBoundary:
            return "exception_boundary";
        case StrongAnchorRole::ClosedLoopCorner:
            return "closed_loop_corner";
    }
    return "split_boundary";
}

}  // namespace Siligen::Domain::Dispensing::ValueObjects
