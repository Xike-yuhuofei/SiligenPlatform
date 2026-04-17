#include "shared/types/TrajectoryTriggerUtils.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using Siligen::Shared::Types::ApplyTriggerMarkersByPosition;
using Siligen::Shared::Types::BuildTrajectoryCumulativeDistances;
using Siligen::Shared::Types::BuildTriggerMarkerPointIndex;
using Siligen::Shared::Types::CountTriggerMarkers;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::ResolveDistanceGuidedTriggerMarkerPlan;
using Siligen::Shared::Types::ResolveIndexedPointTriggerMarkerPlan;
using Siligen::Shared::Types::ResolveSequentialSegmentTriggerMarkerPlan;
using Siligen::Shared::Types::TriggerMarkerCandidateKind;
using Siligen::Shared::Types::TriggerMarkerPlan;
using Siligen::Shared::Types::TriggerMarkerRequest;
using Siligen::Shared::Types::float32;

Siligen::TrajectoryPoint BuildPoint(float32 x, float32 y) {
    Siligen::TrajectoryPoint point;
    point.position = Point2D(x, y);
    return point;
}

std::vector<float32> CollectTriggerDistances(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::vector<float32> distances;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            distances.push_back(point.trigger_position_mm);
        }
    }
    return distances;
}

std::vector<Point2D> CollectTriggerPositions(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::vector<Point2D> positions;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            positions.push_back(point.position);
        }
    }
    return positions;
}

}  // namespace

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionPreservesDistinctMarkersAcrossOverlappedReturnPath) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(0.0f, 0.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(5.0f, 0.0f),
        Point2D(0.0f, 0.0f),
    };
    const std::vector<float32> trigger_distances_mm{0.0f, 5.0f, 10.0f, 15.0f, 20.0f};

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm));
    ASSERT_EQ(CountTriggerMarkers(points), trigger_positions.size());

    const auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), trigger_distances_mm.size());
    for (std::size_t index = 0; index < trigger_distances_mm.size(); ++index) {
        EXPECT_NEAR(actual_distances[index], trigger_distances_mm[index], 1e-4f);
    }

    const auto actual_positions = CollectTriggerPositions(points);
    ASSERT_EQ(actual_positions.size(), trigger_positions.size());
    for (std::size_t index = 0; index < trigger_positions.size(); ++index) {
        EXPECT_NEAR(actual_positions[index].x, trigger_positions[index].x, 1e-4f);
        EXPECT_NEAR(actual_positions[index].y, trigger_positions[index].y, 1e-4f);
    }
}

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionKeepsConsecutiveSamePositionWhenDistanceDiffers) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(0.0f, 0.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(0.0f, 0.0f),
        Point2D(0.0f, 0.0f),
    };
    const std::vector<float32> trigger_distances_mm{0.0f, 20.0f};

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm));
    ASSERT_EQ(CountTriggerMarkers(points), 2U);

    const auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), 2U);
    EXPECT_NEAR(actual_distances[0], 0.0f, 1e-4f);
    EXPECT_NEAR(actual_distances[1], 20.0f, 1e-4f);
}

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionFallsBackToDistanceGuidedOrderForArcLikeCarrier) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(10.0f, 0.0f),
        BuildPoint(7.0710678f, 7.0710678f),
        BuildPoint(0.0f, 10.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(10.0f, 0.0f),
        Point2D(8.6602540f, 5.0f),
        Point2D(5.0f, 8.6602540f),
        Point2D(0.0f, 10.0f),
    };
    const std::vector<float32> trigger_distances_mm{
        0.0f,
        5.2359877f,
        10.4719755f,
        15.7079633f,
    };

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm, 1e-4f, 0.1f));
    ASSERT_EQ(CountTriggerMarkers(points), trigger_positions.size());

    const auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), trigger_distances_mm.size());
    for (std::size_t index = 0; index < trigger_distances_mm.size(); ++index) {
        EXPECT_NEAR(actual_distances[index], trigger_distances_mm[index], 1e-4f);
    }

    const auto actual_positions = CollectTriggerPositions(points);
    ASSERT_EQ(actual_positions.size(), trigger_positions.size());
    for (std::size_t index = 0; index < trigger_positions.size(); ++index) {
        EXPECT_NEAR(actual_positions[index].x, trigger_positions[index].x, 1e-4f);
        EXPECT_NEAR(actual_positions[index].y, trigger_positions[index].y, 1e-4f);
    }
}

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionPrefersForwardSegmentProjectionOverLaterExactRevisit) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 10.0f),
        BuildPoint(7.0f, 9.0f),
        BuildPoint(8.0f, 8.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(8.0f, 8.0f),
        Point2D(9.0f, 9.0f),
    };
    const std::vector<float32> trigger_distances_mm{
        11.3137085f,
        12.7279221f,
    };

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm));
    ASSERT_EQ(CountTriggerMarkers(points), trigger_positions.size());

    const auto actual_positions = CollectTriggerPositions(points);
    ASSERT_EQ(actual_positions.size(), trigger_positions.size());
    EXPECT_NEAR(actual_positions[0].x, 8.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[0].y, 8.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[1].x, 9.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[1].y, 9.0f, 1e-4f);

    const auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), trigger_distances_mm.size());
    EXPECT_NEAR(actual_distances[0], trigger_distances_mm[0], 1e-4f);
    EXPECT_NEAR(actual_distances[1], trigger_distances_mm[1], 1e-4f);
}

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionPrefersForwardSegmentBoundaryOverLaterExactRevisit) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(20.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(10.0f, 10.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(10.0f, 0.0f),
        Point2D(15.0f, 0.0f),
    };
    const std::vector<float32> trigger_distances_mm{
        29.0f,
        29.5f,
    };

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm));
    ASSERT_EQ(CountTriggerMarkers(points), trigger_positions.size());

    const auto actual_positions = CollectTriggerPositions(points);
    ASSERT_EQ(actual_positions.size(), trigger_positions.size());
    EXPECT_NEAR(actual_positions[0].x, 10.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[0].y, 0.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[1].x, 15.0f, 1e-4f);
    EXPECT_NEAR(actual_positions[1].y, 0.0f, 1e-4f);

    const auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), trigger_distances_mm.size());
    EXPECT_NEAR(actual_distances[0], trigger_distances_mm[0], 1e-4f);
    EXPECT_NEAR(actual_distances[1], trigger_distances_mm[1], 1e-4f);
}

TEST(TrajectoryTriggerUtilsTest, ApplyTriggerMarkersByPositionAllowsClosedLoopPhaseShiftWraparound) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(10.0f, 0.0f),
        BuildPoint(10.0f, 10.0f),
        BuildPoint(0.0f, 10.0f),
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
    };
    const std::vector<Point2D> trigger_positions{
        Point2D(0.0f, 0.0f),
        Point2D(5.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 5.0f),
        Point2D(10.0f, 10.0f),
        Point2D(5.0f, 10.0f),
        Point2D(0.0f, 10.0f),
        Point2D(0.0f, 5.0f),
    };
    const std::vector<float32> trigger_distances_mm{
        0.0f,
        5.0f,
        10.0f,
        15.0f,
        20.0f,
        25.0f,
        30.0f,
        35.0f,
    };

    ASSERT_TRUE(ApplyTriggerMarkersByPosition(points, trigger_positions, trigger_distances_mm));
    ASSERT_EQ(CountTriggerMarkers(points), trigger_positions.size());

    auto actual_distances = CollectTriggerDistances(points);
    ASSERT_EQ(actual_distances.size(), trigger_distances_mm.size());
    std::sort(actual_distances.begin(), actual_distances.end());
    for (std::size_t index = 0; index < trigger_distances_mm.size(); ++index) {
        EXPECT_NEAR(actual_distances[index], trigger_distances_mm[index], 1e-4f);
    }
}

TEST(TrajectoryTriggerUtilsTest, ResolveIndexedPointTriggerMarkerPlanFindsLaterExactPointOutsideDistanceNeighborhood) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(10.0f, 10.0f),
        BuildPoint(0.0f, 10.0f),
        BuildPoint(0.0f, 0.0f),
    };
    const auto cumulative = BuildTrajectoryCumulativeDistances(points);
    const auto point_index = BuildTriggerMarkerPointIndex(points, 1e-3f);

    TriggerMarkerRequest request;
    request.position = Point2D(0.0f, 0.0f);
    request.trigger_distance_mm = 25.0f;
    request.pulse_width_us = 3456;
    request.order_index = 2U;

    TriggerMarkerPlan plan;
    EXPECT_FALSE(ResolveDistanceGuidedTriggerMarkerPlan(points, cumulative, request, 1e-3f, plan));
    ASSERT_TRUE(ResolveIndexedPointTriggerMarkerPlan(points, cumulative, point_index, request, 1e-3f, 1U, plan));
    EXPECT_EQ(plan.kind, TriggerMarkerCandidateKind::ExistingPoint);
    EXPECT_EQ(plan.index, 4U);
    EXPECT_NEAR(plan.ratio, 0.0f, 1e-4f);
    EXPECT_NEAR(plan.position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(plan.position.y, 0.0f, 1e-4f);
    EXPECT_NEAR(plan.trigger_distance_mm, 25.0f, 1e-4f);
    EXPECT_EQ(plan.pulse_width_us, 3456U);
    EXPECT_EQ(plan.order_index, 2U);
}

TEST(TrajectoryTriggerUtilsTest, ResolveSequentialSegmentTriggerMarkerPlanFindsLaterSegmentOutsideDistanceNeighborhood) {
    std::vector<Siligen::TrajectoryPoint> points{
        BuildPoint(0.0f, 0.0f),
        BuildPoint(10.0f, 0.0f),
        BuildPoint(10.0f, 10.0f),
        BuildPoint(0.0f, 10.0f),
        BuildPoint(0.0f, 0.0f),
    };
    const auto cumulative = BuildTrajectoryCumulativeDistances(points);

    TriggerMarkerRequest request;
    request.position = Point2D(0.0f, 5.0f);
    request.trigger_distance_mm = 25.0f;
    request.pulse_width_us = 4567;
    request.order_index = 3U;

    TriggerMarkerPlan plan;
    EXPECT_FALSE(ResolveDistanceGuidedTriggerMarkerPlan(points, cumulative, request, 1e-3f, plan));
    ASSERT_TRUE(ResolveSequentialSegmentTriggerMarkerPlan(points, cumulative, request, 1e-3f, 1U, plan));
    EXPECT_EQ(plan.kind, TriggerMarkerCandidateKind::SegmentProjection);
    EXPECT_EQ(plan.index, 4U);
    EXPECT_NEAR(plan.ratio, 0.5f, 1e-4f);
    EXPECT_NEAR(plan.position.x, 0.0f, 1e-4f);
    EXPECT_NEAR(plan.position.y, 5.0f, 1e-4f);
    EXPECT_NEAR(plan.trigger_distance_mm, 25.0f, 1e-4f);
    EXPECT_EQ(plan.pulse_width_us, 4567U);
    EXPECT_EQ(plan.order_index, 3U);
}
