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
    std::vector<std::string> trigger_refs;
    std::vector<Point2D> points;
    SpacingValidationClassification classification = SpacingValidationClassification::Pass;
    float32 min_observed_spacing_mm = 0.0f;
    float32 max_observed_spacing_mm = 0.0f;
    std::string exception_reason;
    std::string blocking_reason;
};

struct DispenseSpan {
    std::string span_id;
    std::string layout_ref;
    std::vector<std::size_t> source_segment_indices;
    std::size_t order_index = 0;
    bool closed = false;
    float32 total_length_mm = 0.0f;
    float32 actual_spacing_mm = 0.0f;
    std::size_t interval_count = 0;
    float32 phase_mm = 0.0f;
    DispenseSpanCurveMode curve_mode = DispenseSpanCurveMode::Line;
    SpacingValidationClassification validation_state = SpacingValidationClassification::Pass;
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

}  // namespace Siligen::Domain::Dispensing::ValueObjects
