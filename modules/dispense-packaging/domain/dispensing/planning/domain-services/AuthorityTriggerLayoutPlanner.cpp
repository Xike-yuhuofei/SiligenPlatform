#include "AuthorityTriggerLayoutPlanner.h"

#include "CurveFlatteningService.h"
#include "PathArcLengthLocator.h"

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
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpan;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanCurveMode;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationOutcome;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
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
constexpr std::size_t kClosedPhaseCandidateCount = 32U;

struct SpanSlice {
    std::vector<ProcessSegment> segments;
    std::vector<std::size_t> source_segment_indices;
    std::vector<float32> segment_lengths_mm;
    float32 start_distance_mm = 0.0f;
};

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

std::vector<SpanSlice> ExtractDispenseSpans(
    const ProcessPath& path,
    const std::vector<float32>& segment_lengths_mm) {
    std::vector<SpanSlice> spans;
    SpanSlice current;
    float32 accumulated_path_length_mm = 0.0f;
    for (std::size_t index = 0; index < path.segments.size(); ++index) {
        const auto& process_segment = path.segments[index];
        const float32 length_mm = index < segment_lengths_mm.size() ? segment_lengths_mm[index] : 0.0f;
        if (process_segment.dispense_on) {
            if (current.segments.empty()) {
                current.start_distance_mm = accumulated_path_length_mm;
            }
            current.segments.push_back(process_segment);
            current.source_segment_indices.push_back(index);
            current.segment_lengths_mm.push_back(length_mm);
        } else if (!current.segments.empty()) {
            spans.push_back(std::move(current));
            current = SpanSlice{};
        }

        accumulated_path_length_mm += length_mm;
    }

    if (!current.segments.empty()) {
        spans.push_back(std::move(current));
    }
    return spans;
}

bool IsClosedSpan(const SpanSlice& span) {
    if (span.segments.empty()) {
        return false;
    }
    if (span.segments.size() == 1U &&
        span.segments.front().geometry.type == SegmentType::Spline &&
        span.segments.front().geometry.spline.closed) {
        return true;
    }
    return SegmentStart(span.segments.front().geometry).DistanceTo(
               SegmentEnd(span.segments.back().geometry)) <= kSharedVertexToleranceMm;
}

DispenseSpanCurveMode ResolveCurveMode(const SpanSlice& span) {
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

float32 ComputeSpanLength(const SpanSlice& span) {
    float32 total_length_mm = 0.0f;
    for (float32 segment_length_mm : span.segment_lengths_mm) {
        total_length_mm += segment_length_mm;
    }
    return total_length_mm;
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

std::vector<float32> BuildSegmentBoundaries(const SpanSlice& span) {
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

float32 WrapLoopDistance(float32 distance_mm, float32 total_length_mm) {
    if (total_length_mm <= kEpsilon) {
        return 0.0f;
    }
    const float32 wrapped = std::fmod(distance_mm, total_length_mm);
    return wrapped < 0.0f ? wrapped + total_length_mm : wrapped;
}

float32 CircularDistance(float32 lhs, float32 rhs, float32 total_length_mm) {
    const float32 direct = std::fabs(lhs - rhs);
    return std::min(direct, std::max(0.0f, total_length_mm - direct));
}

float32 ScoreClosedLoopPhase(
    float32 phase_mm,
    float32 total_length_mm,
    float32 actual_spacing_mm,
    std::size_t interval_count,
    const std::vector<float32>& boundaries) {
    float32 min_clearance_mm = std::numeric_limits<float32>::infinity();
    for (std::size_t index = 0; index < interval_count; ++index) {
        const float32 trigger_distance_mm =
            WrapLoopDistance(phase_mm + actual_spacing_mm * static_cast<float32>(index), total_length_mm);
        min_clearance_mm = std::min(min_clearance_mm, CircularDistance(trigger_distance_mm, 0.0f, total_length_mm));
        for (float32 boundary_mm : boundaries) {
            min_clearance_mm = std::min(
                min_clearance_mm,
                CircularDistance(trigger_distance_mm, boundary_mm, total_length_mm));
        }
    }
    return std::isfinite(min_clearance_mm) ? min_clearance_mm : 0.0f;
}

float32 ResolveClosedLoopPhase(
    float32 total_length_mm,
    float32 actual_spacing_mm,
    std::size_t interval_count,
    const std::vector<float32>& boundaries) {
    if (total_length_mm <= kEpsilon || actual_spacing_mm <= kEpsilon || interval_count == 0U) {
        return 0.0f;
    }

    float32 best_phase_mm = 0.0f;
    float32 best_score_mm = -1.0f;
    const float32 phase_step_mm = actual_spacing_mm / static_cast<float32>(kClosedPhaseCandidateCount);
    for (std::size_t candidate_index = 0; candidate_index < kClosedPhaseCandidateCount; ++candidate_index) {
        const float32 candidate_phase_mm = phase_step_mm * static_cast<float32>(candidate_index);
        const float32 score_mm = ScoreClosedLoopPhase(
            candidate_phase_mm,
            total_length_mm,
            actual_spacing_mm,
            interval_count,
            boundaries);
        if (score_mm > best_score_mm + kEpsilon ||
            (std::fabs(score_mm - best_score_mm) <= kEpsilon && candidate_phase_mm < best_phase_mm)) {
            best_score_mm = score_mm;
            best_phase_mm = candidate_phase_mm;
        }
    }
    return best_phase_mm;
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
    outcome.blocking_reason = blocking_reason;
    return outcome;
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

    const auto spans = ExtractDispenseSpans(request.process_path, segment_lengths_mm);
    if (spans.empty()) {
        layout.layout_id = BuildStableId("layout", request.layout_id_seed + "|empty");
        layout.state = AuthorityTriggerLayoutState::Blocked;
        return Result<AuthorityTriggerLayout>::Success(std::move(layout));
    }

    std::ostringstream layout_seed;
    layout_seed << request.layout_id_seed << '|' << spans.size() << '|' << request.target_spacing_mm;
    layout.layout_id = BuildStableId("layout", layout_seed.str());

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

    std::size_t global_sequence = 0;
    bool has_blocking_failure = false;

    for (std::size_t span_index = 0; span_index < spans.size(); ++span_index) {
        const auto& span = spans[span_index];
        const std::string span_id = BuildStableId("span", layout.layout_id + "|" + std::to_string(span_index));

        DispenseSpan layout_span;
        layout_span.span_id = span_id;
        layout_span.layout_ref = layout.layout_id;
        layout_span.source_segment_indices = span.source_segment_indices;
        layout_span.order_index = span_index;
        layout_span.closed = IsClosedSpan(span);
        layout_span.curve_mode = ResolveCurveMode(span);

        auto append_failed_span = [&](const std::string& reason) {
            layout_span.validation_state = SpacingValidationClassification::Fail;
            has_blocking_failure = true;
            layout.spans.push_back(layout_span);
            layout.validation_outcomes.push_back(BuildBlockingOutcome(layout.layout_id, span_id, reason));
        };

        const float32 total_length_mm = ComputeSpanLength(span);
        if (total_length_mm <= kEpsilon) {
            append_failed_span("authority span length unavailable");
            continue;
        }

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

        const std::size_t division_count = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(std::lround(total_length_mm / request.target_spacing_mm)));
        const float32 actual_spacing_mm = total_length_mm / static_cast<float32>(division_count);
        const auto boundaries = BuildSegmentBoundaries(span);

        layout_span.total_length_mm = total_length_mm;
        layout_span.actual_spacing_mm = actual_spacing_mm;
        layout_span.interval_count = division_count;
        layout_span.phase_mm = layout_span.closed
            ? ResolveClosedLoopPhase(total_length_mm, actual_spacing_mm, division_count, boundaries)
            : 0.0f;

        SpacingValidationOutcome outcome;
        outcome.outcome_id = BuildStableId("spacing", layout_span.span_id);
        outcome.layout_ref = layout.layout_id;
        outcome.span_ref = layout_span.span_id;
        outcome.classification = ResolveClassification(
            actual_spacing_mm,
            layout.min_spacing_mm,
            layout.max_spacing_mm,
            outcome.exception_reason,
            outcome.blocking_reason);
        outcome.min_observed_spacing_mm = actual_spacing_mm;
        outcome.max_observed_spacing_mm = actual_spacing_mm;
        layout_span.validation_state = outcome.classification;
        if (outcome.classification == SpacingValidationClassification::Fail) {
            has_blocking_failure = true;
            layout.spans.push_back(layout_span);
            layout.validation_outcomes.push_back(std::move(outcome));
            continue;
        }

        const std::size_t point_count = layout_span.closed ? division_count : (division_count + 1U);
        std::vector<LayoutTriggerPoint> span_trigger_points;
        span_trigger_points.reserve(point_count);
        bool span_failed = false;
        for (std::size_t point_index = 0; point_index < point_count; ++point_index) {
            const float32 distance_mm = layout_span.closed
                ? WrapLoopDistance(
                    layout_span.phase_mm + actual_spacing_mm * static_cast<float32>(point_index),
                    total_length_mm)
                : ((point_index == division_count)
                    ? total_length_mm
                    : actual_spacing_mm * static_cast<float32>(point_index));
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
            if (!layout_span.closed && (point_index == 0 || point_index + 1 == point_count)) {
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

    layout.authority_ready = !layout.trigger_points.empty() && !has_blocking_failure;
    layout.state = layout.authority_ready
        ? AuthorityTriggerLayoutState::LayoutReady
        : AuthorityTriggerLayoutState::Blocked;
    return Result<AuthorityTriggerLayout>::Success(std::move(layout));
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
