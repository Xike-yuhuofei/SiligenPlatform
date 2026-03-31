#include "application/services/dispensing/PreviewSnapshotService.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

using Siligen::Application::Services::Dispensing::PreviewSnapshotInput;
using Siligen::Application::Services::Dispensing::PreviewSnapshotPayload;
using Siligen::Application::Services::Dispensing::PreviewSnapshotService;
using Siligen::Shared::Types::Point2D;

std::vector<Siligen::TrajectoryPoint> BuildTrajectory(const std::vector<Point2D>& points) {
    std::vector<Siligen::TrajectoryPoint> trajectory;
    trajectory.reserve(points.size());
    for (const auto& point : points) {
        trajectory.emplace_back(point.x, point.y, 0.0f);
    }
    return trajectory;
}

PreviewSnapshotInput BuildInput(const std::vector<Siligen::TrajectoryPoint>& trajectory) {
    PreviewSnapshotInput input;
    input.snapshot_id = "snapshot-1";
    input.snapshot_hash = "fp-snapshot-1";
    input.plan_id = "plan-1";
    input.preview_state = "snapshot_ready";
    input.confirmed_at = "2026-03-28T00:00:00Z";
    input.segment_count = static_cast<std::uint32_t>(trajectory.size() > 1 ? trajectory.size() - 1 : 0);
    input.point_count = static_cast<std::uint32_t>(trajectory.size());
    input.total_length_mm = 30.0f;
    input.estimated_time_s = 1.0f;
    input.generated_at = "2026-03-28T00:00:01Z";
    input.trajectory_points = &trajectory;
    return input;
}

bool SnapshotContainsPoint(
    const PreviewSnapshotPayload& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.trajectory_polyline) {
        if (std::abs(point.x - target_x) <= tolerance && std::abs(point.y - target_y) <= tolerance) {
            return true;
        }
    }
    return false;
}

std::size_t CountSnapshotPointsNear(
    const PreviewSnapshotPayload& snapshot,
    float target_x,
    float target_y,
    float radius) {
    std::size_t count = 0U;
    for (const auto& point : snapshot.trajectory_polyline) {
        const double dx = static_cast<double>(point.x) - static_cast<double>(target_x);
        const double dy = static_cast<double>(point.y) - static_cast<double>(target_y);
        if (std::sqrt(dx * dx + dy * dy) <= static_cast<double>(radius)) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST(PreviewSnapshotServiceTest, BuildPayloadPreservesMetadataAndBuildsPreviewPolyline) {
    PreviewSnapshotService service;
    const auto trajectory = BuildTrajectory({Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildPayload(input, 16);

    EXPECT_EQ(payload.snapshot_id, "snapshot-1");
    EXPECT_EQ(payload.snapshot_hash, "fp-snapshot-1");
    EXPECT_EQ(payload.plan_id, "plan-1");
    EXPECT_EQ(payload.preview_state, "snapshot_ready");
    EXPECT_EQ(payload.confirmed_at, "2026-03-28T00:00:00Z");
    EXPECT_EQ(payload.polyline_source_point_count, 2U);
    EXPECT_EQ(payload.polyline_point_count, payload.trajectory_polyline.size());
}

TEST(PreviewSnapshotServiceTest, BuildPayloadSuppressesShortABATailArtifacts) {
    PreviewSnapshotService service;
    const auto trajectory = BuildTrajectory({
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(10.2f, 10.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
    });
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildPayload(input, 64);

    EXPECT_FALSE(SnapshotContainsPoint(payload, 10.2f, 10.0f, 1e-4f));
    EXPECT_FALSE(SnapshotContainsPoint(payload, 10.0f, 10.0f, 1e-4f));
    EXPECT_EQ(CountSnapshotPointsNear(payload, 10.0f, 10.0f, 2.1f), 2U);
    EXPECT_TRUE(SnapshotContainsPoint(payload, 0.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(payload, 0.0f, 10.0f, 1e-4f));
}

TEST(PreviewSnapshotServiceTest, BuildPayloadKeepsCornerWhenDownsampling) {
    PreviewSnapshotService service;
    std::vector<Point2D> raw_points;
    for (int i = 0; i <= 9; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }
    for (int j = 1; j <= 10; ++j) {
        raw_points.emplace_back(9.0f, static_cast<float>(j));
    }
    const auto trajectory = BuildTrajectory(raw_points);
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildPayload(input, 6);

    EXPECT_LE(payload.trajectory_polyline.size(), 6U);
    EXPECT_TRUE(SnapshotContainsPoint(payload, 9.0f, 0.0f, 1e-4f));
}

TEST(PreviewSnapshotServiceTest, BuildPayloadUsesThreeMillimeterCenterSpacing) {
    PreviewSnapshotService service;
    std::vector<Point2D> raw_points;
    for (int i = 0; i <= 30; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }
    const auto trajectory = BuildTrajectory(raw_points);
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildPayload(input, 128);

    ASSERT_GE(payload.trajectory_polyline.size(), 2U);
    EXPECT_NEAR(payload.trajectory_polyline.front().x, 0.0f, 1e-4f);
    EXPECT_NEAR(payload.trajectory_polyline.back().x, 30.0f, 1e-4f);
    EXPECT_EQ(payload.trajectory_polyline.size(), 11U);
    for (std::size_t i = 1; i < payload.trajectory_polyline.size(); ++i) {
        const auto& prev = payload.trajectory_polyline[i - 1U];
        const auto& curr = payload.trajectory_polyline[i];
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        const double distance = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(distance, 3.0, 1e-2) << "spacing at segment index " << i;
    }
}

TEST(PreviewSnapshotServiceTest, BuildPayloadDoesNotForceCornerVerticesIntoFixedSpacingPreview) {
    PreviewSnapshotService service;
    const auto trajectory = BuildTrajectory({
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 102.0f),
        Point2D(0.0f, 102.0f),
        Point2D(0.0f, 0.0f),
    });
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildPayload(input, 256);

    EXPECT_FALSE(SnapshotContainsPoint(payload, 100.0f, 0.0f, 1e-4f));
    EXPECT_FALSE(SnapshotContainsPoint(payload, 100.0f, 102.0f, 1e-4f));
    EXPECT_FALSE(SnapshotContainsPoint(payload, 0.0f, 102.0f, 1e-4f));
    EXPECT_LE(CountSnapshotPointsNear(payload, 100.0f, 0.0f, 3.5f), 2U);
    EXPECT_LE(CountSnapshotPointsNear(payload, 100.0f, 102.0f, 3.5f), 2U);
    EXPECT_LE(CountSnapshotPointsNear(payload, 0.0f, 102.0f, 3.5f), 2U);
}
