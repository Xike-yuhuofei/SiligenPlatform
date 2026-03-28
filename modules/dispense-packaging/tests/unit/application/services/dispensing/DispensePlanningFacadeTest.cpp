#include "application/services/dispensing/DispensePlanningFacade.h"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using Siligen::Application::Services::Dispensing::DispensePlanningFacade;
using Siligen::Application::Services::Dispensing::PlanningArtifactsBuildInput;
using Siligen::Domain::Motion::InterpolationAlgorithm;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using Siligen::Domain::Trajectory::ValueObjects::Segment;
using Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Shared::Types::Point2D;

MotionTrajectoryPoint BuildMotionPoint(float t, float x, float y, bool dispense_on = true) {
    MotionTrajectoryPoint point;
    point.t = t;
    point.position = Siligen::Point3D(x, y, 0.0f);
    point.velocity = Siligen::Point3D(10.0f, 0.0f, 0.0f);
    point.dispense_on = dispense_on;
    return point;
}

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

ProcessSegment BuildPointSegment(const Point2D& point, bool dispense_on = true) {
    Segment segment;
    segment.type = SegmentType::Line;
    segment.line.start = point;
    segment.line.end = point;
    segment.length = 0.0f;
    segment.is_point = true;

    ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = dispense_on;
    return process_segment;
}

PlanningArtifactsBuildInput BuildInput() {
    PlanningArtifactsBuildInput input;
    input.source_path = "sample.pb";
    input.dxf_filename = "sample.pb";
    input.dispensing_velocity = 10.0f;
    input.acceleration = 200.0f;
    input.dispenser_interval_ms = 100;
    input.dispenser_duration_ms = 100;
    input.trigger_spatial_interval_mm = 5.0f;
    input.sample_dt = 0.01f;
    input.max_jerk = 5000.0f;
    input.dispensing_strategy = DispensingStrategy::BASELINE;
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };
    input.motion_plan.total_length = 10.0f;
    input.motion_plan.total_time = 1.0f;
    input.estimated_time_s = 1.25f;
    return input;
}

bool HasConsecutiveNearDuplicatePoints(
    const std::vector<Point2D>& points,
    float tolerance_mm) {
    for (std::size_t index = 1; index < points.size(); ++index) {
        if (points[index - 1].DistanceTo(points[index]) <= tolerance_mm) {
            return true;
        }
    }
    return false;
}

std::size_t CountTriggerMarkers(const std::vector<Siligen::TrajectoryPoint>& points) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            ++count;
        }
    }
    return count;
}

}  // namespace

TEST(DispensePlanningFacadeTest, AssemblePlanningArtifactsProducesValidatedExecutionPackage) {
    DispensePlanningFacade facade;

    const auto result = facade.AssemblePlanningArtifacts(BuildInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.execution_package.source_path, "sample.pb");
    EXPECT_FLOAT_EQ(payload.execution_package.total_length_mm, 10.0f);
    EXPECT_FLOAT_EQ(payload.execution_package.estimated_time_s, 1.25f);
    EXPECT_TRUE(payload.execution_package.Validate().IsSuccess());
    EXPECT_FALSE(payload.execution_package.execution_plan.interpolation_segments.empty());
}

TEST(DispensePlanningFacadeTest, AssemblePlanningArtifactsBuildsPreviewPayloadAndExportRequest) {
    DispensePlanningFacade facade;

    const auto result = facade.AssemblePlanningArtifacts(BuildInput());

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    EXPECT_EQ(payload.segment_count, 1);
    EXPECT_FLOAT_EQ(payload.total_length, 10.0f);
    EXPECT_FLOAT_EQ(payload.estimated_time, 1.25f);
    EXPECT_FALSE(payload.trajectory_points.empty());
    EXPECT_FALSE(payload.glue_points.empty());
    EXPECT_EQ(payload.trigger_count, static_cast<int>(payload.glue_points.size()));
    EXPECT_EQ(payload.export_request.source_path, "sample.pb");
    EXPECT_EQ(payload.export_request.dxf_filename, "sample.pb");
    EXPECT_EQ(payload.export_request.glue_points.size(), payload.glue_points.size());
    EXPECT_EQ(payload.export_request.execution_trajectory_points.size(), payload.trajectory_points.size());
}

TEST(DispensePlanningFacadeTest, AssemblePlanningArtifactsBuildsGluePointsFromTriggerDistances) {
    auto input = BuildInput();
    input.trigger_spatial_interval_mm = 3.0f;
    input.motion_plan.points.clear();
    for (int index = 0; index <= 100; ++index) {
        const float ratio = static_cast<float>(index) / 100.0f;
        input.motion_plan.points.push_back(BuildMotionPoint(ratio, ratio * 10.0f, 0.0f));
    }

    DispensePlanningFacade facade;
    const auto result = facade.AssemblePlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_GT(payload.trajectory_points.size(), payload.glue_points.size());
    ASSERT_EQ(payload.glue_points.size(), 3U);
    EXPECT_EQ(payload.trigger_count, 3);
    EXPECT_NEAR(payload.glue_points[0].x, 3.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[1].x, 6.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[2].x, 9.0f, 1e-4f);
}

TEST(DispensePlanningFacadeTest, AssemblePlanningArtifactsLinearInterpolationOnlyMarksDiscreteTriggerPoints) {
    auto input = BuildInput();
    input.use_interpolation_planner = true;
    input.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    input.trigger_spatial_interval_mm = 3.0f;
    input.max_jerk = 500.0f;
    input.sample_ds = 10.0f;

    DispensePlanningFacade facade;
    const auto result = facade.AssemblePlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 3U);
    EXPECT_EQ(CountTriggerMarkers(payload.trajectory_points), payload.glue_points.size());
    EXPECT_LT(payload.glue_points.size(), payload.trajectory_points.size());
}

TEST(DispensePlanningFacadeTest, AssemblePlanningArtifactsDeduplicatesRepeatedTriggerPointsAtSegmentBoundaries) {
    auto input = BuildInput();
    input.process_path.segments.clear();
    input.process_path.segments.push_back(BuildLineSegment(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    input.process_path.segments.push_back(BuildPointSegment(Point2D(10.0f, 0.0f)));
    input.motion_plan.points = {
        BuildMotionPoint(0.0f, 0.0f, 0.0f),
        BuildMotionPoint(1.0f, 10.0f, 0.0f),
    };

    DispensePlanningFacade facade;
    const auto result = facade.AssemblePlanningArtifacts(input);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& payload = result.Value();
    ASSERT_EQ(payload.glue_points.size(), 2U);
    EXPECT_FALSE(HasConsecutiveNearDuplicatePoints(payload.glue_points, 1e-4f));
    EXPECT_NEAR(payload.glue_points[0].x, 5.0f, 1e-4f);
    EXPECT_NEAR(payload.glue_points[1].x, 10.0f, 1e-4f);
}
