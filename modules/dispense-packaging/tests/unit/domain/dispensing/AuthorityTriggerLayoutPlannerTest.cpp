#include "domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlanner;
using Siligen::Domain::Dispensing::DomainServices::AuthorityTriggerLayoutPlannerRequest;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;

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

TEST(AuthorityTriggerLayoutPlannerTest, PlansClosedLoopWithStablePhaseAndNoDuplicateTail) {
    AuthorityTriggerLayoutPlanner planner;

    auto first_request = BuildRequest();
    first_request.target_spacing_mm = 5.0f;
    first_request.spline_max_step_mm = 1.0f;
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)));
    first_request.process_path.segments.push_back(
        BuildLineSegment(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)));

    auto rotated_request = BuildRequest();
    rotated_request.target_spacing_mm = 5.0f;
    rotated_request.spline_max_step_mm = 1.0f;
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
    EXPECT_TRUE(first_layout.spans.front().closed);
    EXPECT_EQ(first_layout.trigger_points.size(), first_layout.spans.front().interval_count);
    EXPECT_GT(first_layout.spans.front().phase_mm, 0.0f);
    EXPECT_LT(first_layout.spans.front().phase_mm, first_layout.spans.front().actual_spacing_mm);
    EXPECT_FALSE(first_layout.trigger_points.empty());
    EXPECT_GT(
        first_layout.trigger_points.front().position.DistanceTo(first_layout.trigger_points.back().position),
        1e-3f);
    EXPECT_NEAR(
        first_layout.spans.front().phase_mm,
        rotated_layout.spans.front().phase_mm,
        1e-4f);

    auto normalize = [](const auto& layout) {
        std::vector<std::pair<float, float>> points;
        points.reserve(layout.trigger_points.size());
        for (const auto& trigger : layout.trigger_points) {
            points.emplace_back(trigger.position.x, trigger.position.y);
        }
        std::sort(points.begin(), points.end());
        return points;
    };
    EXPECT_EQ(normalize(first_layout), normalize(rotated_layout));
}
