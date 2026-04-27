#include "application/services/dispensing/PreviewSnapshotService.h"
#include "domain/dispensing/value-objects/AuthorityTriggerLayout.h"
#include "process_path/contracts/ProcessPath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

using Siligen::Application::Services::Dispensing::PreviewSnapshotInput;
using Siligen::Application::Services::Dispensing::PreviewSnapshotResponse;
using Siligen::Application::Services::Dispensing::PreviewSnapshotService;
using Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayout;
using Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding;
using Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint;
using Siligen::Engineering::Contracts::PathQuality;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

std::vector<Siligen::TrajectoryPoint> BuildTrajectory(const std::vector<Point2D>& points) {
    std::vector<Siligen::TrajectoryPoint> trajectory;
    trajectory.reserve(points.size());
    for (const auto& point : points) {
        trajectory.emplace_back(point.x, point.y, 0.0f);
    }
    return trajectory;
}

const PathQuality& DefaultPathQuality() {
    static const PathQuality path_quality{
        "pass",
        false,
        {},
        "path_quality passed",
        "runtime_authority_path_quality",
        {true, "pass", "", "", 0},
        "runtime_path_quality.v1",
    };
    return path_quality;
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
    input.motion_trajectory_points = &trajectory;
    input.path_quality = &DefaultPathQuality();
    return input;
}

PreviewSnapshotInput BuildProcessPathInput(const ProcessPath& process_path) {
    PreviewSnapshotInput input;
    input.snapshot_id = "snapshot-process-path";
    input.snapshot_hash = "fp-process-path";
    input.plan_id = "plan-process-path";
    input.preview_state = "snapshot_ready";
    input.confirmed_at = "2026-03-28T00:00:00Z";
    input.segment_count = static_cast<std::uint32_t>(process_path.segments.size());
    input.point_count = 0U;
    input.total_length_mm = 12.0f;
    input.estimated_time_s = 1.0f;
    input.generated_at = "2026-03-28T00:00:01Z";
    input.process_path = &process_path;
    input.path_quality = &DefaultPathQuality();
    return input;
}

AuthorityTriggerLayout BuildAuthorityTriggerLayout(
    const std::string& layout_id,
    const std::vector<Point2D>& points) {
    AuthorityTriggerLayout layout;
    layout.layout_id = layout_id;
    layout.plan_id = "plan-1";
    layout.plan_fingerprint = "fp-snapshot-1";
    layout.authority_ready = true;
    layout.binding_ready = true;

    float cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm += static_cast<float>(
                points[index - 1U].DistanceTo(points[index]));
        }

        LayoutTriggerPoint trigger_point;
        trigger_point.trigger_id = layout_id + "-trigger-" + std::to_string(index);
        trigger_point.layout_ref = layout_id;
        trigger_point.sequence_index_global = index;
        trigger_point.distance_mm_global = cumulative_length_mm;
        trigger_point.position = points[index];
        layout.trigger_points.push_back(trigger_point);

        InterpolationTriggerBinding binding;
        binding.binding_id = layout_id + "-binding-" + std::to_string(index);
        binding.layout_ref = layout_id;
        binding.trigger_ref = trigger_point.trigger_id;
        binding.interpolation_index = index;
        binding.execution_position = points[index];
        binding.match_error_mm = 0.0f;
        binding.monotonic = true;
        binding.bound = true;
        layout.bindings.push_back(binding);
    }

    return layout;
}

bool SnapshotContainsPoint(
    const PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.motion_preview_polyline) {
        if (std::abs(point.x - target_x) <= tolerance && std::abs(point.y - target_y) <= tolerance) {
            return true;
        }
    }
    return false;
}

std::size_t CountSnapshotPointsNear(
    const PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float radius) {
    std::size_t count = 0U;
    for (const auto& point : snapshot.motion_preview_polyline) {
        const double dx = static_cast<double>(point.x) - static_cast<double>(target_x);
        const double dy = static_cast<double>(point.y) - static_cast<double>(target_y);
        if (std::sqrt(dx * dx + dy * dy) <= static_cast<double>(radius)) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST(PreviewSnapshotServiceTest, BuildResponsePreservesMetadataAndBuildsPreviewPolyline) {
    PreviewSnapshotService service;
    const auto trajectory = BuildTrajectory({Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildResponse(input, 16, 8);

    EXPECT_EQ(payload.snapshot_id, "snapshot-1");
    EXPECT_EQ(payload.snapshot_hash, "fp-snapshot-1");
    EXPECT_EQ(payload.plan_id, "plan-1");
    EXPECT_EQ(payload.preview_state, "snapshot_ready");
    EXPECT_EQ(payload.confirmed_at, "2026-03-28T00:00:00Z");
    EXPECT_EQ(payload.motion_preview_source_point_count, 2U);
    EXPECT_EQ(payload.motion_preview_point_count, payload.motion_preview_polyline.size());
    EXPECT_EQ(payload.path_quality.verdict, "pass");
    EXPECT_FALSE(payload.path_quality.blocking);
}

TEST(PreviewSnapshotServiceTest, BuildResponsePreservesExecutionTrajectoryVerticesWhenWithinClampBudget) {
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

    const auto payload = service.BuildResponse(input, 64, 8);

    EXPECT_EQ(payload.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving");
    EXPECT_FALSE(payload.motion_preview_is_sampled);
    EXPECT_EQ(payload.motion_preview_source_point_count, trajectory.size());
    EXPECT_EQ(payload.motion_preview_point_count, trajectory.size());
    EXPECT_TRUE(SnapshotContainsPoint(payload, 10.2f, 10.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(payload, 10.0f, 10.0f, 1e-4f));
    EXPECT_EQ(CountSnapshotPointsNear(payload, 10.0f, 10.0f, 2.1f), 3U);
    EXPECT_TRUE(SnapshotContainsPoint(payload, 0.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(payload, 0.0f, 10.0f, 1e-4f));
}

TEST(PreviewSnapshotServiceTest, BuildResponseKeepsCornerWhenDownsampling) {
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

    const auto payload = service.BuildResponse(input, 6, 8);

    EXPECT_LE(payload.motion_preview_polyline.size(), 6U);
    EXPECT_TRUE(SnapshotContainsPoint(payload, 9.0f, 0.0f, 1e-4f));
}

TEST(PreviewSnapshotServiceTest, BuildResponsePreservesDenseExecutionTrajectoryWhenWithinClampBudget) {
    PreviewSnapshotService service;
    std::vector<Point2D> raw_points;
    for (int i = 0; i <= 30; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }
    const auto trajectory = BuildTrajectory(raw_points);
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildResponse(input, 128, 8);

    ASSERT_GE(payload.motion_preview_polyline.size(), 2U);
    EXPECT_EQ(payload.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving");
    EXPECT_FALSE(payload.motion_preview_is_sampled);
    EXPECT_NEAR(payload.motion_preview_polyline.front().x, 0.0f, 1e-4f);
    EXPECT_NEAR(payload.motion_preview_polyline.back().x, 30.0f, 1e-4f);
    EXPECT_EQ(payload.motion_preview_polyline.size(), 31U);
    for (std::size_t i = 1; i < payload.motion_preview_polyline.size(); ++i) {
        const auto& prev = payload.motion_preview_polyline[i - 1U];
        const auto& curr = payload.motion_preview_polyline[i];
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        const double distance = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(distance, 1.0, 1e-2) << "spacing at segment index " << i;
    }
}

TEST(PreviewSnapshotServiceTest, BuildResponsePreservesCornerVerticesWhenWithinClampBudget) {
    PreviewSnapshotService service;
    const auto trajectory = BuildTrajectory({
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 102.0f),
        Point2D(0.0f, 102.0f),
        Point2D(0.0f, 0.0f),
    });
    const auto input = BuildInput(trajectory);

    const auto payload = service.BuildResponse(input, 256, 8);

    EXPECT_EQ(payload.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving");
    EXPECT_FALSE(payload.motion_preview_is_sampled);
    EXPECT_TRUE(SnapshotContainsPoint(payload, 100.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(payload, 100.0f, 102.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(payload, 0.0f, 102.0f, 1e-4f));
    EXPECT_EQ(payload.motion_preview_point_count, trajectory.size());
}

TEST(PreviewSnapshotServiceTest, BuildResponseDoesNotFallbackToProcessPathForMotionPreview) {
    PreviewSnapshotService service;
    Segment line_segment;
    line_segment.type = SegmentType::Line;
    line_segment.line.start = Point2D(0.0f, 0.0f);
    line_segment.line.end = Point2D(12.0f, 0.0f);
    line_segment.length = 12.0f;

    ProcessSegment process_segment;
    process_segment.geometry = line_segment;

    ProcessPath process_path;
    process_path.segments.push_back(process_segment);

    const auto input = BuildProcessPathInput(process_path);

    const auto payload = service.BuildResponse(input, 64, 8);

    EXPECT_TRUE(payload.motion_preview_source.empty());
    EXPECT_TRUE(payload.motion_preview_kind.empty());
    EXPECT_EQ(payload.motion_preview_source_point_count, 0U);
    EXPECT_EQ(payload.motion_preview_point_count, 0U);
    EXPECT_TRUE(payload.motion_preview_polyline.empty());
}

TEST(PreviewSnapshotServiceTest, BuildResponsePublishesReadyPreviewBindingFromAuthorityExecutionTruth) {
    PreviewSnapshotService service;
    const std::vector<Point2D> glue_points = {
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(20.0f, 0.0f),
    };
    const auto trajectory = BuildTrajectory(glue_points);
    auto input = BuildInput(trajectory);
    auto authority_trigger_layout = BuildAuthorityTriggerLayout("layout-ready", glue_points);
    input.glue_points = &glue_points;
    input.authority_trigger_layout = &authority_trigger_layout;
    input.authority_layout_id = authority_trigger_layout.layout_id;
    input.binding_ready = true;

    const auto payload = service.BuildResponse(input, 32, 32);

    ASSERT_EQ(payload.glue_points.size(), 3U);
    EXPECT_EQ(payload.preview_binding.source, "runtime_authority_preview_binding");
    EXPECT_EQ(payload.preview_binding.status, "ready");
    EXPECT_EQ(payload.preview_binding.layout_id, "layout-ready");
    EXPECT_EQ(payload.preview_binding.glue_point_count, payload.glue_points.size());
    EXPECT_EQ(payload.preview_binding.binding_basis, "execution_binding_to_motion_preview_polyline");
    EXPECT_FLOAT_EQ(payload.preview_binding.display_path_length_mm, 20.0f);
    ASSERT_EQ(payload.preview_binding.source_trigger_indices.size(), 3U);
    EXPECT_EQ(payload.preview_binding.source_trigger_indices[0], 0U);
    EXPECT_EQ(payload.preview_binding.source_trigger_indices[1], 1U);
    EXPECT_EQ(payload.preview_binding.source_trigger_indices[2], 2U);
    ASSERT_EQ(payload.preview_binding.display_reveal_lengths_mm.size(), 3U);
    EXPECT_FLOAT_EQ(payload.preview_binding.display_reveal_lengths_mm[0], 0.0f);
    EXPECT_FLOAT_EQ(payload.preview_binding.display_reveal_lengths_mm[1], 10.0f);
    EXPECT_FLOAT_EQ(payload.preview_binding.display_reveal_lengths_mm[2], 20.0f);
    EXPECT_TRUE(payload.preview_binding.failure_reason.empty());
}

TEST(PreviewSnapshotServiceTest, BuildResponseMarksPreviewBindingUnavailableWhenAuthorityBindingsAreIncomplete) {
    PreviewSnapshotService service;
    const std::vector<Point2D> glue_points = {
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(20.0f, 0.0f),
    };
    const auto trajectory = BuildTrajectory(glue_points);
    auto input = BuildInput(trajectory);
    auto authority_trigger_layout = BuildAuthorityTriggerLayout("layout-unavailable", glue_points);
    authority_trigger_layout.bindings.pop_back();
    input.glue_points = &glue_points;
    input.authority_trigger_layout = &authority_trigger_layout;
    input.authority_layout_id = authority_trigger_layout.layout_id;
    input.binding_ready = true;

    const auto payload = service.BuildResponse(input, 32, 32);

    EXPECT_EQ(payload.preview_binding.source, "runtime_authority_preview_binding");
    EXPECT_EQ(payload.preview_binding.status, "unavailable");
    EXPECT_EQ(payload.preview_binding.layout_id, "layout-unavailable");
    EXPECT_EQ(payload.preview_binding.failure_reason, "preview binding unavailable");
    EXPECT_TRUE(payload.preview_binding.source_trigger_indices.empty());
    EXPECT_TRUE(payload.preview_binding.display_reveal_lengths_mm.empty());
}
