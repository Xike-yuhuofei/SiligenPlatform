#include "shared/types/TrajectoryTriggerUtils.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

using Siligen::Shared::Types::ApplyTriggerMarkersByPosition;
using Siligen::Shared::Types::CountTriggerMarkers;
using Siligen::Shared::Types::Point2D;
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
