#include "domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h"
#include "domain/dispensing/planning/domain-services/TopologyComponentClassifier.h"
#include "process_path/contracts/GeometryUtils.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlanner;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlannerRequest;
using Siligen::Domain::Dispensing::DomainServices::TopologyComponentClassifier;
using Siligen::Domain::Dispensing::DomainServices::TopologySpanSlice;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanCurveMode;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanPhaseStrategy;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason;
using Siligen::Domain::Dispensing::ValueObjects::DispenseSpanTopologyType;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerSourceKind;
using Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification;
using Siligen::Domain::Dispensing::ValueObjects::StrongAnchorRole;
using Siligen::Domain::Dispensing::ValueObjects::TopologyDispatchType;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;

constexpr char kMixedExplicitBoundaryWithReorderedBranchFamily[] =
    "mixed_explicit_boundary_with_reordered_branch_family";

ProcessSegment BuildLineSegment(const Point2D& start, const Point2D& end, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = start;
    segment.line.end = end;
    segment.length = start.DistanceTo(end);

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

ProcessSegment BuildArcSegment(
    const Point2D& center,
    float radius,
    float start_angle_deg,
    float end_angle_deg,
    bool clockwise = false,
    bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Arc;
    segment.arc.center = center;
    segment.arc.radius = radius;
    segment.arc.start_angle_deg = start_angle_deg;
    segment.arc.end_angle_deg = end_angle_deg;
    segment.arc.clockwise = clockwise;
    segment.length = ComputeArcLength(segment.arc);

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

ProcessSegment BuildSplineSegment(const std::vector<Point2D>& control_points, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Spline;
    segment.spline.control_points = control_points;
    segment.spline.closed = false;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

AuthorityTriggerLayoutPlannerRequest BuildRequest() {
    AuthorityTriggerLayoutPlannerRequest request;
    request.layout_id_seed = "planner-test";
    request.target_spacing_mm = 3.0f;
    request.min_spacing_mm = 2.7f;
    request.max_spacing_mm = 3.3f;
    request.dispensing_velocity = 10.0f;
    request.acceleration = 200.0f;
    request.dispenser_interval_ms = 100;
    request.dispenser_duration_ms = 100;
    request.dispensing_strategy = DispensingStrategy::BASELINE;
    return request;
}

void ExpectSameLayoutPositions(
    const Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout& lhs,
    const Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout& rhs) {
    ASSERT_EQ(lhs.trigger_points.size(), rhs.trigger_points.size());
    for (std::size_t index = 0; index < lhs.trigger_points.size(); ++index) {
        EXPECT_NEAR(lhs.trigger_points[index].position.x, rhs.trigger_points[index].position.x, 1e-4f);
        EXPECT_NEAR(lhs.trigger_points[index].position.y, rhs.trigger_points[index].position.y, 1e-4f);
        EXPECT_NEAR(lhs.trigger_points[index].distance_mm_global, rhs.trigger_points[index].distance_mm_global, 1e-4f);
    }
}

std::vector<std::pair<float, float>> NormalizeTriggerPositions(
    const Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout& layout) {
    std::vector<std::pair<float, float>> points;
    points.reserve(layout.trigger_points.size());
    for (const auto& trigger : layout.trigger_points) {
        points.emplace_back(trigger.position.x, trigger.position.y);
    }
    std::sort(points.begin(), points.end());
    return points;
}

std::size_t CountAnchorRoles(
    const Siligen::Domain::Dispensing::ValueObjects::DispenseSpan& span,
    StrongAnchorRole role) {
    return static_cast<std::size_t>(std::count_if(
        span.strong_anchors.begin(),
        span.strong_anchors.end(),
        [&](const auto& anchor) { return anchor.role == role; }));
}

std::size_t CountSourceKind(
    const std::vector<LayoutTriggerPoint>& trigger_points,
    const std::string& span_id,
    LayoutTriggerSourceKind source_kind) {
    return static_cast<std::size_t>(std::count_if(
        trigger_points.begin(),
        trigger_points.end(),
        [&](const auto& point) {
            return point.span_ref == span_id && point.source_kind == source_kind;
        }));
}

std::size_t CountPointsNear(
    const std::vector<LayoutTriggerPoint>& trigger_points,
    const std::string& span_id,
    const Point2D& position,
    float tolerance_mm) {
    return static_cast<std::size_t>(std::count_if(
        trigger_points.begin(),
        trigger_points.end(),
        [&](const auto& point) {
            return point.span_ref == span_id && point.position.DistanceTo(position) <= tolerance_mm;
        }));
}

}  // namespace

TEST(AuthorityTriggerLayoutPlannerTest, PlansEquivalentLayoutForSingleAndSubdividedOpenSpan) {
    AuthorityTriggerLayoutPlanner planner;

    auto single_request = BuildRequest();
    single_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));

    auto subdivided_request = BuildRequest();
    subdivided_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(5.0f, 0.0f)));
    subdivided_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(5.0f, 0.0f), Point2D(10.0f, 0.0f)));

    const auto single_result = planner.Plan(single_request);
    const auto subdivided_result = planner.Plan(subdivided_request);

    ASSERT_TRUE(single_result.IsSuccess()) << single_result.GetError().GetMessage();
    ASSERT_TRUE(subdivided_result.IsSuccess()) << subdivided_result.GetError().GetMessage();
    ExpectSameLayoutPositions(single_result.Value(), subdivided_result.Value());
}

TEST(AuthorityTriggerLayoutPlannerTest, SolvesGlobalSpacingAcrossOpenSpan) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    ASSERT_EQ(layout.trigger_points.size(), 4U);
    EXPECT_NEAR(layout.trigger_points[0].position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[1].position.x, 10.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[2].position.x, 20.0f / 3.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[3].position.x, 10.0f, 1e-4f);
}

TEST(AuthorityTriggerLayoutPlannerTest, PreservesGlobalDistancesAcrossSeparatedDispenseSpans) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(6.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(6.0f, 0.0f), Point2D(10.0f, 0.0f), false));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(16.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 2U);
    ASSERT_EQ(layout.trigger_points.size(), 6U);
    EXPECT_NEAR(layout.trigger_points[0].distance_mm_global, 0.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[1].distance_mm_global, 3.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[2].distance_mm_global, 6.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[3].distance_mm_global, 10.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[4].distance_mm_global, 13.0f, 1e-4f);
    EXPECT_NEAR(layout.trigger_points[5].distance_mm_global, 16.0f, 1e-4f);
}

TEST(AuthorityTriggerLayoutPlannerTest, ProducesStableLayoutAcrossRepeatedPlanning) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(5.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(5.0f, 0.0f), Point2D(10.0f, 0.0f)));

    const auto first = planner.Plan(request);
    const auto second = planner.Plan(request);

    ASSERT_TRUE(first.IsSuccess()) << first.GetError().GetMessage();
    ASSERT_TRUE(second.IsSuccess()) << second.GetError().GetMessage();
    EXPECT_EQ(first.Value().layout_id, second.Value().layout_id);
    ExpectSameLayoutPositions(first.Value(), second.Value());
}

TEST(AuthorityTriggerLayoutPlannerTest, PlansClosedLoopWithStableCornerAnchorsAcrossRotatedStarts) {
    AuthorityTriggerLayoutPlanner planner;

    auto first_request = BuildRequest();
    first_request.target_spacing_mm = 5.0f;
    first_request.min_spacing_mm = 4.7f;
    first_request.max_spacing_mm = 5.3f;
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));

    auto rotated_request = first_request;
    rotated_request.process_path.segments.clear();
    rotated_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    rotated_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    rotated_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    rotated_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));

    const auto first_result = planner.Plan(first_request);
    const auto rotated_result = planner.Plan(rotated_request);

    ASSERT_TRUE(first_result.IsSuccess()) << first_result.GetError().GetMessage();
    ASSERT_TRUE(rotated_result.IsSuccess()) << rotated_result.GetError().GetMessage();

    const auto& first_layout = first_result.Value();
    const auto& rotated_layout = rotated_result.Value();
    ASSERT_EQ(first_layout.spans.size(), 1U);
    ASSERT_EQ(rotated_layout.spans.size(), 1U);
    const auto& first_span = first_layout.spans.front();
    const auto& rotated_span = rotated_layout.spans.front();
    EXPECT_TRUE(first_span.closed);
    EXPECT_EQ(first_span.topology_type, DispenseSpanTopologyType::ClosedLoop);
    EXPECT_EQ(first_span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_EQ(first_span.interval_count, 8U);
    EXPECT_FLOAT_EQ(first_span.phase_mm, 0.0f);
    EXPECT_TRUE(first_span.anchor_constraints_satisfied);
    EXPECT_EQ(first_span.candidate_corner_count, 4U);
    EXPECT_EQ(first_span.accepted_corner_count, 4U);
    EXPECT_EQ(CountAnchorRoles(first_span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(CountSourceKind(first_layout.trigger_points, first_span.span_id, LayoutTriggerSourceKind::Anchor), 4U);
    EXPECT_EQ(first_layout.trigger_points.size(), first_span.interval_count);
    EXPECT_GT(
        first_layout.trigger_points.front().position.DistanceTo(first_layout.trigger_points.back().position),
        1e-3f);
    EXPECT_EQ(first_span.phase_mm, rotated_span.phase_mm);
    EXPECT_EQ(first_span.interval_count, rotated_span.interval_count);
    EXPECT_EQ(NormalizeTriggerPositions(first_layout), NormalizeTriggerPositions(rotated_layout));
}

TEST(AuthorityTriggerLayoutPlannerTest, SelectsAnchorConstrainedRectangleSolutionWithinDerivedCornerTolerance) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(100.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(100.0f, 0.0f), Point2D(100.0f, 100.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(100.0f, 100.0f), Point2D(0.0f, 100.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 100.0f), Point2D(0.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    const auto& span = layout.spans.front();
    const float anchor_tolerance_mm = std::max(1e-4f, request.target_spacing_mm * 0.25f);
    EXPECT_TRUE(span.closed);
    EXPECT_EQ(span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_GT(span.interval_count, 0U);
    EXPECT_NEAR(span.actual_spacing_mm, request.target_spacing_mm, 0.1f);
    EXPECT_GE(span.phase_mm, 0.0f);
    EXPECT_LT(span.phase_mm, span.actual_spacing_mm);
    EXPECT_EQ(span.validation_state, SpacingValidationClassification::Pass);
    EXPECT_TRUE(span.anchor_constraints_satisfied);
    EXPECT_EQ(span.candidate_corner_count, 4U);
    EXPECT_EQ(span.accepted_corner_count, 4U);
    EXPECT_EQ(span.suppressed_corner_count, 0U);
    EXPECT_EQ(CountAnchorRoles(span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(0.0f, 0.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(100.0f, 0.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(100.0f, 100.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(0.0f, 100.0f), anchor_tolerance_mm + 1e-3f), 1U);
}

TEST(AuthorityTriggerLayoutPlannerTest, FallsBackToPhaseSearchWhenClosedLoopCornerAnchorsAreDisabled) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.enable_closed_loop_corner_anchors = false;
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    const auto& span = layout.spans.front();
    EXPECT_TRUE(span.closed);
    EXPECT_EQ(span.phase_strategy, DispenseSpanPhaseStrategy::DeterministicSearch);
    EXPECT_EQ(span.anchor_policy, Siligen::Domain::Dispensing::ValueObjects::DispenseSpanAnchorPolicy::ClosedLoopPhaseSearch);
    EXPECT_EQ(span.candidate_corner_count, 0U);
    EXPECT_EQ(span.accepted_corner_count, 0U);
    EXPECT_EQ(span.suppressed_corner_count, 0U);
    EXPECT_EQ(CountAnchorRoles(span, StrongAnchorRole::ClosedLoopCorner), 0U);
    EXPECT_FALSE(span.anchor_constraints_satisfied);
    EXPECT_LT(span.phase_mm, span.actual_spacing_mm);
    EXPECT_EQ(CountSourceKind(layout.trigger_points, span.span_id, LayoutTriggerSourceKind::Anchor), 0U);
}

TEST(AuthorityTriggerLayoutPlannerTest, PrefersInWindowAnchorConstrainedSolutionWhenPhaseCanSatisfyCorners) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 4.0f;
    request.min_spacing_mm = 3.9f;
    request.max_spacing_mm = 4.1f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    ASSERT_EQ(layout.validation_outcomes.size(), 1U);
    const auto& span = layout.spans.front();
    const auto& outcome = layout.validation_outcomes.front();
    const float anchor_tolerance_mm = std::max(1e-4f, request.target_spacing_mm * 0.25f);
    EXPECT_TRUE(span.closed);
    EXPECT_EQ(span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_EQ(span.validation_state, SpacingValidationClassification::Pass);
    EXPECT_EQ(outcome.classification, SpacingValidationClassification::Pass);
    EXPECT_EQ(span.interval_count, 10U);
    EXPECT_NEAR(span.actual_spacing_mm, 4.0f, 1e-4f);
    EXPECT_GE(span.phase_mm, 0.0f);
    EXPECT_LT(span.phase_mm, span.actual_spacing_mm);
    EXPECT_TRUE(span.anchor_constraints_satisfied);
    EXPECT_EQ(outcome.anchor_constraint_state, "satisfied");
    EXPECT_TRUE(outcome.anchor_exception_reason.empty());
    EXPECT_TRUE(outcome.exception_reason.empty());
    EXPECT_EQ(CountAnchorRoles(span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(0.0f, 0.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(10.0f, 0.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(10.0f, 10.0f), anchor_tolerance_mm + 1e-3f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, span.span_id, Point2D(0.0f, 10.0f), anchor_tolerance_mm + 1e-3f), 1U);
}

TEST(AuthorityTriggerLayoutPlannerTest, UsesSpacingDerivedDefaultToleranceForIrregularClosedLoopCornerAnchors) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(3.4f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(3.4f, 0.0f), Point2D(2.3739395f, 2.8190779f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(2.3739395f, 2.8190779f), Point2D(-0.82287604f, 2.6763549f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(-0.82287604f, 2.6763549f), Point2D(0.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    ASSERT_EQ(layout.validation_outcomes.size(), 1U);
    const auto& span = layout.spans.front();
    const auto& outcome = layout.validation_outcomes.front();
    EXPECT_TRUE(layout.authority_ready);
    EXPECT_EQ(layout.state, Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState::LayoutReady);
    EXPECT_TRUE(span.closed);
    EXPECT_EQ(span.topology_type, DispenseSpanTopologyType::ClosedLoop);
    EXPECT_EQ(span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_EQ(span.validation_state, SpacingValidationClassification::Pass);
    EXPECT_EQ(outcome.classification, SpacingValidationClassification::Pass);
    EXPECT_TRUE(span.anchor_constraints_satisfied);
    EXPECT_EQ(outcome.anchor_constraint_state, "satisfied");
    EXPECT_EQ(span.candidate_corner_count, 4U);
    EXPECT_EQ(span.accepted_corner_count, 4U);
    EXPECT_EQ(CountAnchorRoles(span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(span.interval_count, 4U);
    EXPECT_GT(span.phase_mm, 0.0f);
    EXPECT_LT(span.phase_mm, span.actual_spacing_mm);
    EXPECT_EQ(CountSourceKind(layout.trigger_points, span.span_id, LayoutTriggerSourceKind::Anchor), 4U);
}

TEST(AuthorityTriggerLayoutPlannerTest, SplitsBranchRevisitCandidateIntoClosedLoopAndOpenSpan) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 10.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_TRUE(layout.topology_dispatch_applied);
    ASSERT_TRUE(layout.branch_revisit_split_applied);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);

    const auto& closed_span = layout.spans[0];
    const auto& diagonal_span = layout.spans[1];
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_FALSE(layout.components.front().ignored);
    EXPECT_EQ(layout.components.front().span_refs.size(), 2U);
    EXPECT_EQ(closed_span.component_id, layout.components.front().component_id);
    EXPECT_EQ(diagonal_span.component_id, layout.components.front().component_id);
    EXPECT_TRUE(closed_span.closed);
    EXPECT_EQ(closed_span.topology_type, DispenseSpanTopologyType::ClosedLoop);
    EXPECT_EQ(closed_span.split_reason, DispenseSpanSplitReason::BranchOrRevisit);
    EXPECT_EQ(closed_span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_EQ(closed_span.source_segment_indices, (std::vector<std::size_t>{0U, 1U, 2U, 3U}));
    EXPECT_EQ(CountAnchorRoles(closed_span, StrongAnchorRole::SplitBoundary), 1U);
    EXPECT_EQ(CountAnchorRoles(closed_span, StrongAnchorRole::ClosedLoopCorner), 4U);
    EXPECT_EQ(closed_span.accepted_corner_count, 4U);
    EXPECT_TRUE(closed_span.anchor_constraints_satisfied);
    EXPECT_FLOAT_EQ(closed_span.phase_mm, 0.0f);

    EXPECT_FALSE(diagonal_span.closed);
    EXPECT_EQ(diagonal_span.topology_type, DispenseSpanTopologyType::OpenChain);
    EXPECT_EQ(diagonal_span.split_reason, DispenseSpanSplitReason::BranchOrRevisit);
    EXPECT_EQ(diagonal_span.source_segment_indices, (std::vector<std::size_t>{4U}));
    EXPECT_TRUE(std::any_of(
        diagonal_span.strong_anchors.begin(),
        diagonal_span.strong_anchors.end(),
        [](const auto& anchor) { return anchor.role == StrongAnchorRole::SplitBoundary; }));

    const auto diagonal_trigger = std::find_if(
        layout.trigger_points.begin(),
        layout.trigger_points.end(),
        [&](const auto& trigger) {
            return trigger.span_ref == diagonal_span.span_id && trigger.sequence_index_span == 0U;
        });
    ASSERT_NE(diagonal_trigger, layout.trigger_points.end());
    EXPECT_NEAR(diagonal_trigger->position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(diagonal_trigger->position.y, 0.0f, 1e-4f);
    EXPECT_EQ(diagonal_trigger->source_kind, LayoutTriggerSourceKind::Anchor);
}

TEST(AuthorityTriggerLayoutPlannerTest, SplitsDisconnectedContoursIntoIndependentMultiContourComponents) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(20.0f, 0.0f), Point2D(30.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(30.0f, 0.0f), Point2D(30.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(30.0f, 10.0f), Point2D(20.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(20.0f, 10.0f), Point2D(20.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::MultiContour);
    EXPECT_EQ(layout.effective_component_count, 2U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 2U);
    ASSERT_EQ(layout.spans.size(), 2U);

    EXPECT_EQ(layout.components[0].dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(layout.components[1].dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_FALSE(layout.components[0].ignored);
    EXPECT_FALSE(layout.components[1].ignored);
    EXPECT_EQ(layout.components[0].span_refs.size(), 1U);
    EXPECT_EQ(layout.components[1].span_refs.size(), 1U);

    EXPECT_EQ(layout.spans[0].component_id, layout.components[0].component_id);
    EXPECT_EQ(layout.spans[1].component_id, layout.components[1].component_id);
    EXPECT_EQ(layout.spans[0].dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(layout.spans[1].dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(layout.spans[0].split_reason, DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_EQ(layout.spans[1].split_reason, DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_TRUE(layout.spans[0].closed);
    EXPECT_TRUE(layout.spans[1].closed);
}

TEST(AuthorityTriggerLayoutPlannerTest, IgnoresShortAuxiliaryOpenComponentWhenPrimaryContourExists) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 2.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(25.0f, 0.0f), Point2D(27.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::AuxiliaryGeometry);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 1U);
    ASSERT_EQ(layout.components.size(), 2U);
    ASSERT_EQ(layout.spans.size(), 1U);

    EXPECT_EQ(layout.components[0].dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_FALSE(layout.components[0].ignored);
    EXPECT_EQ(layout.components[1].dispatch_type, TopologyDispatchType::AuxiliaryGeometry);
    EXPECT_TRUE(layout.components[1].ignored);
    EXPECT_FALSE(layout.components[1].ignored_reason.empty());

    EXPECT_EQ(layout.spans.front().component_id, layout.components[0].component_id);
    EXPECT_TRUE(layout.spans.front().closed);
    EXPECT_EQ(layout.spans.front().dispatch_type, TopologyDispatchType::SingleClosedLoop);
    EXPECT_EQ(
        std::count_if(
            layout.trigger_points.begin(),
            layout.trigger_points.end(),
            [](const auto& trigger) {
                return trigger.position.DistanceTo(Point2D(25.0f, 0.0f)) <= 1e-4f ||
                    trigger.position.DistanceTo(Point2D(27.0f, 0.0f)) <= 1e-4f;
            }),
        0);
}

TEST(AuthorityTriggerLayoutPlannerTest, ClassifiesRapidSeparatedSharedVertexSpansAsExplicitProcessBoundary) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    EXPECT_TRUE(layout.authority_ready);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);
    ASSERT_EQ(layout.trigger_points.size(), 6U);

    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_FALSE(layout.components.front().ignored);
    EXPECT_TRUE(layout.components.front().blocking_reason.empty());
    EXPECT_EQ(layout.components.front().span_refs.size(), 2U);
    EXPECT_EQ(layout.spans[0].component_id, layout.components.front().component_id);
    EXPECT_EQ(layout.spans[1].component_id, layout.components.front().component_id);
    EXPECT_EQ(layout.spans[0].split_reason, DispenseSpanSplitReason::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[1].split_reason, DispenseSpanSplitReason::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[0].dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(layout.spans[1].dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_FALSE(layout.spans[0].closed);
    EXPECT_FALSE(layout.spans[1].closed);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, layout.spans[0].span_id, Point2D(0.0f, 0.0f), 1e-4f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, layout.spans[1].span_id, Point2D(0.0f, 0.0f), 1e-4f), 1U);
}

TEST(AuthorityTriggerLayoutPlannerTest, ClassifiesMixedSingleOpenChainAndExplicitBoundaryFamilyAsExplicitProcessBoundary) {
    TopologyComponentClassifier classifier;

    TopologySpanSlice open_chain_span;
    open_chain_span.segments = {
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)),
    };
    open_chain_span.source_segment_indices = {0U, 1U};
    open_chain_span.segment_lengths_mm = {10.0f, 10.0f};
    open_chain_span.start_distance_mm = 0.0f;
    open_chain_span.split_reason = DispenseSpanSplitReason::None;

    TopologySpanSlice explicit_boundary_span;
    explicit_boundary_span.segments = {
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)),
    };
    explicit_boundary_span.source_segment_indices = {2U};
    explicit_boundary_span.segment_lengths_mm = {10.0f};
    explicit_boundary_span.start_distance_mm = 25.0f;
    explicit_boundary_span.split_reason = DispenseSpanSplitReason::ExplicitProcessBoundary;

    const auto result = classifier.Classify({
        {open_chain_span, explicit_boundary_span},
        1e-4f,
        4.7f,
    });

    EXPECT_EQ(result.dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_EQ(result.effective_component_count, 1U);
    EXPECT_EQ(result.ignored_component_count, 0U);
    ASSERT_EQ(result.components.size(), 1U);
    EXPECT_EQ(result.components.front().dispatch_type, TopologyDispatchType::ExplicitProcessBoundary);
    EXPECT_TRUE(result.components.front().blocking_reason.empty());
    ASSERT_EQ(result.components.front().spans.size(), 2U);
    EXPECT_EQ(result.components.front().spans[0].split_reason, DispenseSpanSplitReason::None);
    EXPECT_EQ(result.components.front().spans[1].split_reason, DispenseSpanSplitReason::ExplicitProcessBoundary);
}

TEST(AuthorityTriggerLayoutPlannerTest, ReclassifiesSharedVertexReorderedSpansAsBranchOrRevisitComponent) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    EXPECT_TRUE(layout.authority_ready);
    EXPECT_FALSE(layout.branch_revisit_split_applied);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 2U);
    ASSERT_EQ(layout.trigger_points.size(), 6U);

    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_FALSE(layout.components.front().ignored);
    EXPECT_TRUE(layout.components.front().blocking_reason.empty());
    EXPECT_EQ(layout.components.front().span_refs.size(), 2U);
    EXPECT_EQ(layout.spans[0].component_id, layout.components.front().component_id);
    EXPECT_EQ(layout.spans[1].component_id, layout.components.front().component_id);
    EXPECT_EQ(layout.spans[0].split_reason, DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_EQ(layout.spans[1].split_reason, DispenseSpanSplitReason::MultiContourBoundary);
    EXPECT_EQ(layout.spans[0].dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_EQ(layout.spans[1].dispatch_type, TopologyDispatchType::BranchOrRevisit);
    EXPECT_FALSE(layout.spans[0].closed);
    EXPECT_FALSE(layout.spans[1].closed);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, layout.spans[0].span_id, Point2D(0.0f, 0.0f), 1e-4f), 1U);
    EXPECT_EQ(CountPointsNear(layout.trigger_points, layout.spans[1].span_id, Point2D(0.0f, 0.0f), 1e-4f), 1U);
}

TEST(AuthorityTriggerLayoutPlannerTest, KeepsExplicitBoundaryMixedWithSharedVertexReorderBlocked) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 5.0f;
    request.min_spacing_mm = 4.7f;
    request.max_spacing_mm = 5.3f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(0.0f, 0.0f), false));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(-10.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    EXPECT_FALSE(layout.authority_ready);
    EXPECT_EQ(layout.state, Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState::Blocked);
    EXPECT_EQ(layout.dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
    EXPECT_EQ(layout.effective_component_count, 1U);
    EXPECT_EQ(layout.ignored_component_count, 0U);
    ASSERT_EQ(layout.components.size(), 1U);
    ASSERT_EQ(layout.spans.size(), 3U);
    ASSERT_EQ(layout.validation_outcomes.size(), 3U);
    EXPECT_TRUE(layout.trigger_points.empty());
    EXPECT_EQ(layout.components.front().dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
    EXPECT_FALSE(layout.components.front().ignored);
    EXPECT_EQ(layout.components.front().blocking_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
    EXPECT_EQ(layout.components.front().span_refs.size(), 3U);
    for (const auto& outcome : layout.validation_outcomes) {
        EXPECT_EQ(outcome.classification, SpacingValidationClassification::Fail);
        EXPECT_EQ(outcome.dispatch_type, TopologyDispatchType::UnsupportedMixedTopology);
        EXPECT_EQ(outcome.blocking_reason, kMixedExplicitBoundaryWithReorderedBranchFamily);
        EXPECT_EQ(outcome.component_id, layout.components.front().component_id);
    }
}

TEST(AuthorityTriggerLayoutPlannerTest, ClustersDenseClosedLoopCornersBeforeAnchorConstrainedLayout) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.target_spacing_mm = 1.0f;
    request.min_spacing_mm = 0.9f;
    request.max_spacing_mm = 1.1f;
    request.closed_loop_corner_cluster_distance_mm = 1.5f;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(1.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(1.0f, 0.0f), Point2D(1.0f, 1.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(1.0f, 1.0f), Point2D(10.0f, 1.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 1.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    const auto& span = layout.spans.front();
    EXPECT_TRUE(span.closed);
    EXPECT_EQ(span.phase_strategy, DispenseSpanPhaseStrategy::AnchorConstrained);
    EXPECT_EQ(span.validation_state, SpacingValidationClassification::Pass);
    EXPECT_GT(span.candidate_corner_count, span.accepted_corner_count);
    EXPECT_GT(span.suppressed_corner_count, 0U);
    EXPECT_GT(span.dense_corner_cluster_count, 0U);
    EXPECT_EQ(span.interval_count, 40U);
    EXPECT_TRUE(span.anchor_constraints_satisfied);
}

TEST(AuthorityTriggerLayoutPlannerTest, KeepsSmoothArcClosedLoopOnPhaseSearchWithoutCornerAnchors) {
    AuthorityTriggerLayoutPlanner planner;

    auto first_request = BuildRequest();
    first_request.target_spacing_mm = 5.0f;
    first_request.min_spacing_mm = 4.7f;
    first_request.max_spacing_mm = 5.3f;
    first_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 0.0f, 90.0f));
    first_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 90.0f, 180.0f));
    first_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 180.0f, 270.0f));
    first_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 270.0f, 360.0f));

    auto rotated_request = first_request;
    rotated_request.process_path.segments.clear();
    rotated_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 90.0f, 180.0f));
    rotated_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 180.0f, 270.0f));
    rotated_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 270.0f, 360.0f));
    rotated_request.process_path.segments.push_back(BuildArcSegment(Point2D(0.0f, 0.0f), 10.0f, 0.0f, 90.0f));

    const auto first_result = planner.Plan(first_request);
    const auto rotated_result = planner.Plan(rotated_request);

    ASSERT_TRUE(first_result.IsSuccess()) << first_result.GetError().GetMessage();
    ASSERT_TRUE(rotated_result.IsSuccess()) << rotated_result.GetError().GetMessage();

    const auto& first_layout = first_result.Value();
    const auto& rotated_layout = rotated_result.Value();
    ASSERT_EQ(first_layout.spans.size(), 1U);
    ASSERT_EQ(rotated_layout.spans.size(), 1U);
    const auto& first_span = first_layout.spans.front();
    const auto& rotated_span = rotated_layout.spans.front();
    EXPECT_TRUE(first_span.closed);
    EXPECT_EQ(first_span.curve_mode, DispenseSpanCurveMode::Arc);
    EXPECT_EQ(first_span.phase_strategy, DispenseSpanPhaseStrategy::DeterministicSearch);
    EXPECT_EQ(first_span.accepted_corner_count, 0U);
    EXPECT_EQ(CountAnchorRoles(first_span, StrongAnchorRole::ClosedLoopCorner), 0U);
    EXPECT_FALSE(first_span.anchor_constraints_satisfied);
    EXPECT_LT(first_span.phase_mm, first_span.actual_spacing_mm);
    EXPECT_EQ(first_span.phase_mm, rotated_span.phase_mm);
    EXPECT_EQ(first_span.interval_count, rotated_span.interval_count);
    EXPECT_EQ(first_layout.trigger_points.size(), rotated_layout.trigger_points.size());
    for (const auto& trigger : first_layout.trigger_points) {
        EXPECT_NEAR(trigger.position.DistanceTo(Point2D(0.0f, 0.0f)), 10.0f, 1e-3f);
    }
    for (const auto& trigger : rotated_layout.trigger_points) {
        EXPECT_NEAR(trigger.position.DistanceTo(Point2D(0.0f, 0.0f)), 10.0f, 1e-3f);
    }
}

TEST(AuthorityTriggerLayoutPlannerTest, CanDisableBranchRevisitSplitForLegacyComparison) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.enable_branch_revisit_split = false;
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));
    request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 10.0f)));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    EXPECT_FALSE(layout.branch_revisit_split_applied);
    EXPECT_FALSE(layout.spans.front().closed);
}

TEST(AuthorityTriggerLayoutPlannerTest, KeepsSplineOpenChainOnUnifiedArcLengthKernel) {
    AuthorityTriggerLayoutPlanner planner;
    auto request = BuildRequest();
    request.spline_max_error_mm = 0.05f;
    request.spline_max_step_mm = 1.0f;
    request.process_path.segments.push_back(BuildSplineSegment({
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 5.0f),
        Point2D(10.0f, 0.0f),
    }));

    const auto result = planner.Plan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& layout = result.Value();
    ASSERT_EQ(layout.spans.size(), 1U);
    EXPECT_FALSE(layout.branch_revisit_split_applied);
    EXPECT_EQ(layout.spans.front().split_reason, DispenseSpanSplitReason::None);
    EXPECT_EQ(layout.spans.front().curve_mode, DispenseSpanCurveMode::FlattenedCurve);
    EXPECT_FALSE(layout.trigger_points.empty());
}
