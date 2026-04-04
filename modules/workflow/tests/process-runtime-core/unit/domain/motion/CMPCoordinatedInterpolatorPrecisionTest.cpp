#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/domain-services/TrajectoryPlanner.h"
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/GeometryUtils.h"

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {
constexpr float kEpsilon = 1e-4f;
constexpr float kPi = 3.14159265358979323846f;

float ComputeTestArcLength(const Siligen::ProcessPath::Contracts::ArcPrimitive& arc) {
    float sweep_deg = arc.end_angle_deg - arc.start_angle_deg;
    if (arc.clockwise && sweep_deg > 0.0f) {
        sweep_deg -= 360.0f;
    } else if (!arc.clockwise && sweep_deg < 0.0f) {
        sweep_deg += 360.0f;
    }
    return std::abs(sweep_deg) * kPi / 180.0f * arc.radius;
}

Siligen::Shared::Types::Point2D ComputeTestArcPoint(
    const Siligen::ProcessPath::Contracts::ArcPrimitive& arc,
    float angle_deg) {
    const float angle_rad = angle_deg * kPi / 180.0f;
    return Siligen::Shared::Types::Point2D(
        arc.center.x + arc.radius * std::cos(angle_rad),
        arc.center.y + arc.radius * std::sin(angle_rad));
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

class ArcPathSourceStub : public Siligen::Domain::Trajectory::Ports::IPathSourcePort {
   public:
    explicit ArcPathSourceStub(Siligen::ProcessPath::Contracts::Primitive primitive)
        : primitive_(std::move(primitive)) {}

    Siligen::Shared::Types::Result<Siligen::Domain::Trajectory::Ports::PathSourceResult> LoadFromFile(
        const std::string& filepath) override {
        (void)filepath;

        Siligen::Domain::Trajectory::Ports::PathSourceResult result;
        result.success = true;
        result.primitives.push_back(primitive_);

        Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta meta;
        meta.entity_id = 1;
        result.metadata.push_back(meta);
        return Siligen::Shared::Types::Result<Siligen::Domain::Trajectory::Ports::PathSourceResult>::Success(result);
    }

   private:
    Siligen::ProcessPath::Contracts::Primitive primitive_;
};
}  // namespace

TEST(CMPCoordinatedInterpolatorPrecisionTest, InsertsExactTriggerPointInsteadOfSnappingToNearestSample) {
    using Siligen::CMPConfiguration;
    using Siligen::Point2D;
    using Siligen::TrajectoryPoint;
    using Siligen::TriggerTimeline;
    using Siligen::Domain::Motion::TrajectoryPlanner;

    std::vector<TrajectoryPoint> base_trajectory;
    base_trajectory.push_back(TrajectoryPoint(Point2D(0.0f, 0.0f), 10.0f));
    base_trajectory.back().timestamp = 0.0f;
    base_trajectory.push_back(TrajectoryPoint(Point2D(10.0f, 0.0f), 10.0f));
    base_trajectory.back().timestamp = 1.0f;

    TriggerTimeline timeline;
    timeline.trigger_times.push_back(0.25f);
    timeline.trigger_positions.push_back(Point2D(2.5f, 0.0f));
    timeline.pulse_widths.push_back(2000);
    timeline.cmp_channels.push_back(1);

    CMPConfiguration cmp_config;
    TrajectoryPlanner planner;
    auto result = planner.GenerateCMPCoordinatedTrajectory(base_trajectory, timeline, cmp_config);

    ASSERT_EQ(result.size(), 3u);
    ASSERT_TRUE(result[1].enable_position_trigger);
    EXPECT_NEAR(result[1].timestamp, 0.25f, kEpsilon);
    EXPECT_NEAR(result[1].position.x, 2.5f, kEpsilon);
    EXPECT_NEAR(result[1].position.y, 0.0f, kEpsilon);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, AcceptsExternalTriggerListForProcessPathCMPPlanning) {
    using Siligen::CMPConfiguration;
    using Siligen::CMPTriggerMode;
    using Siligen::DispensingTriggerPoint;
    using Siligen::InterpolationConfig;
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::CMPCoordinatedInterpolator;
    using Siligen::ProcessPath::Contracts::ArcPrimitive;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

    ArcPrimitive arc;
    arc.center = Point2D(0.0f, 0.0f);
    arc.radius = 10.0f;
    arc.start_angle_deg = 0.0f;
    arc.end_angle_deg = 90.0f;
    arc.clockwise = false;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.geometry.type = SegmentType::Arc;
    seg.geometry.arc = arc;
    seg.geometry.length = ComputeTestArcLength(arc);
    path.segments.push_back(seg);

    DispensingTriggerPoint trigger;
    trigger.position = ComputeTestArcPoint(arc, 45.0f);
    trigger.trigger_distance = ComputeTestArcLength(arc) * 0.5f;
    trigger.sequence_id = 0;
    trigger.pulse_width_us = 2000;
    trigger.is_enabled = true;

    CMPConfiguration cmp_config;
    cmp_config.trigger_mode = CMPTriggerMode::POSITION_SYNC;
    cmp_config.cmp_channel = 1;
    cmp_config.pulse_width_us = 2000;
    cmp_config.trigger_position_tolerance = 0.1f;
    cmp_config.time_tolerance_ms = 1.0f;
    cmp_config.enable_compensation = true;
    cmp_config.compensation_factor = 1.0f;
    cmp_config.enable_multi_channel = false;
    ASSERT_TRUE(cmp_config.trigger_points.empty());

    InterpolationConfig config;
    config.max_velocity = 20.0f;
    config.max_acceleration = 100.0f;
    config.max_jerk = 500.0f;
    config.time_step = 0.01f;
    config.position_tolerance = 0.01f;

    CMPCoordinatedInterpolator interpolator;
    auto result = interpolator.PositionTriggeredDispensing(path, {trigger}, cmp_config, config);

    ASSERT_FALSE(result.empty());

    std::vector<Point2D> trigger_points;
    for (const auto& point : result) {
        if (point.enable_position_trigger) {
            trigger_points.push_back(point.position);
        }
    }

    ASSERT_FALSE(trigger_points.empty());
    EXPECT_NEAR(trigger_points.front().x, trigger.position.x, kEpsilon);
    EXPECT_NEAR(trigger_points.front().y, trigger.position.y, kEpsilon);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, DispensingPlannerKeepsArcTriggerOnRealArcGeometry) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
    using Siligen::Domain::Motion::InterpolationAlgorithm;
    using Siligen::ProcessPath::Contracts::ArcPoint;
    using Siligen::ProcessPath::Contracts::ArcPrimitive;
    using Siligen::ProcessPath::Contracts::ComputeArcLength;
    using Siligen::ProcessPath::Contracts::Primitive;

    ArcPrimitive arc;
    arc.center = Point2D(0.0f, 0.0f);
    arc.radius = 10.0f;
    arc.start_angle_deg = 0.0f;
    arc.end_angle_deg = 90.0f;
    arc.clockwise = false;

    auto path_source = std::make_shared<ArcPathSourceStub>(
        Primitive::MakeArc(arc.center, arc.radius, arc.start_angle_deg, arc.end_angle_deg, arc.clockwise));
    DispensingPlanner planner(path_source);

    DispensingPlanRequest request;
    request.dxf_filepath = "arc-test.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.max_jerk = 500.0f;
    request.sample_dt = 0.01f;
    request.spline_max_step_mm = 100.0f;
    request.trigger_spatial_interval_mm = ComputeArcLength(arc) * 0.5f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = InterpolationAlgorithm::CMP_COORDINATED;
    request.use_hardware_trigger = false;

    auto plan_result = planner.Plan(request);
    ASSERT_TRUE(plan_result.IsSuccess()) << plan_result.GetError().GetMessage();

    const auto& points = plan_result.Value().interpolation_points;
    ASSERT_FALSE(points.empty());

    std::vector<Point2D> trigger_points;
    for (const auto& point : points) {
        if (point.enable_position_trigger) {
            trigger_points.push_back(point.position);
        }
    }

    ASSERT_FALSE(trigger_points.empty());

    const Point2D expected = ArcPoint(arc, 45.0f);
    float best_distance = std::numeric_limits<float>::max();
    Point2D best_point;
    for (const auto& point : trigger_points) {
        const float distance = point.DistanceTo(expected);
        if (distance < best_distance) {
            best_distance = distance;
            best_point = point;
        }
    }

    EXPECT_NEAR(best_point.x, expected.x, kEpsilon);
    EXPECT_NEAR(best_point.y, expected.y, kEpsilon);
    EXPECT_LT(best_distance, 1e-3f);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, DispensingPlannerLinearInterpolationOnlyMarksRealTriggerDistances) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
    using Siligen::Domain::Motion::InterpolationAlgorithm;
    using Siligen::ProcessPath::Contracts::Primitive;

    auto path_source =
        std::make_shared<ArcPathSourceStub>(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    DispensingPlanner planner(path_source);

    DispensingPlanRequest request;
    request.dxf_filepath = "line-test.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.max_jerk = 500.0f;
    request.sample_dt = 0.01f;
    request.spline_max_step_mm = 100.0f;
    request.trigger_spatial_interval_mm = 3.0f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    request.use_hardware_trigger = false;

    auto plan_result = planner.Plan(request);
    ASSERT_TRUE(plan_result.IsSuccess()) << plan_result.GetError().GetMessage();

    const auto& plan = plan_result.Value();
    ASSERT_FALSE(plan.interpolation_points.empty());
    ASSERT_EQ(plan.trigger_distances_mm.size(), 4U);
    EXPECT_EQ(CountTriggerMarkers(plan.interpolation_points), plan.trigger_distances_mm.size());
    EXPECT_LT(plan.trigger_distances_mm.size(), plan.interpolation_points.size());
    EXPECT_TRUE(plan.preview_authority_ready);
    EXPECT_TRUE(plan.preview_authority_shared_with_execution);
    EXPECT_TRUE(plan.preview_spacing_valid);
    EXPECT_NEAR(plan.trigger_distances_mm.front(), 0.0f, kEpsilon);
    EXPECT_NEAR(plan.trigger_distances_mm.back(), 10.0f, kEpsilon);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, BuildPreviewPointsUsesExplicitInterpolationTriggersOnly) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;

    DispensingPlan plan;
    plan.preview_authority_ready = true;
    plan.preview_authority_shared_with_execution = true;
    plan.preview_spacing_valid = true;
    for (int index = 0; index <= 10; ++index) {
        Siligen::TrajectoryPoint point(Point2D(static_cast<float>(index), 0.0f), 10.0f);
        point.timestamp = static_cast<float>(index);
        point.enable_position_trigger = (index == 0 || index == 5 || index == 10);
        point.trigger_position_mm = static_cast<float>(index);
        plan.interpolation_points.push_back(point);
    }
    plan.trigger_distances_mm = {3.0f, 6.0f, 9.0f};

    DispensingPlanner planner(nullptr);
    const auto preview = planner.BuildPreviewPoints(plan, 3.0f, 1024);

    ASSERT_EQ(preview.size(), 3U);
    EXPECT_TRUE(preview[0].enable_position_trigger);
    EXPECT_TRUE(preview[1].enable_position_trigger);
    EXPECT_TRUE(preview[2].enable_position_trigger);
    EXPECT_NEAR(preview[0].position.x, 0.0f, kEpsilon);
    EXPECT_NEAR(preview[1].position.x, 5.0f, kEpsilon);
    EXPECT_NEAR(preview[2].position.x, 10.0f, kEpsilon);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, DispensingPlannerMarksShortSegmentsAsSpacingExceptions) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
    using Siligen::Domain::Motion::InterpolationAlgorithm;
    using Siligen::ProcessPath::Contracts::Primitive;

    auto path_source =
        std::make_shared<ArcPathSourceStub>(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(2.0f, 0.0f)));
    DispensingPlanner planner(path_source);

    DispensingPlanRequest request;
    request.dxf_filepath = "short-line.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.max_jerk = 500.0f;
    request.sample_dt = 0.01f;
    request.spline_max_step_mm = 100.0f;
    request.trigger_spatial_interval_mm = 3.0f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    request.use_hardware_trigger = false;

    auto plan_result = planner.Plan(request);
    ASSERT_TRUE(plan_result.IsSuccess()) << plan_result.GetError().GetMessage();

    const auto& plan = plan_result.Value();
    ASSERT_EQ(plan.trigger_distances_mm.size(), 2U);
    EXPECT_TRUE(plan.preview_authority_ready);
    EXPECT_TRUE(plan.preview_spacing_valid);
    EXPECT_TRUE(plan.preview_has_short_segment_exceptions);
    ASSERT_EQ(plan.spacing_validation_groups.size(), 1U);
    EXPECT_TRUE(plan.spacing_validation_groups.front().short_segment_exception);
}

TEST(CMPCoordinatedInterpolatorPrecisionTest, DispensingPlannerRejectsZeroLengthDispenseSegments) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
    using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
    using Siligen::Domain::Motion::InterpolationAlgorithm;
    using Siligen::ProcessPath::Contracts::Primitive;

    auto path_source =
        std::make_shared<ArcPathSourceStub>(Primitive::MakeLine(Point2D(1.0f, 1.0f), Point2D(1.0f, 1.0f)));
    DispensingPlanner planner(path_source);

    DispensingPlanRequest request;
    request.dxf_filepath = "point-line.dxf";
    request.dispensing_velocity = 20.0f;
    request.acceleration = 100.0f;
    request.max_jerk = 500.0f;
    request.sample_dt = 0.01f;
    request.spline_max_step_mm = 100.0f;
    request.trigger_spatial_interval_mm = 3.0f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = InterpolationAlgorithm::LINEAR;
    request.use_hardware_trigger = false;

    auto plan_result = planner.Plan(request);
    ASSERT_TRUE(plan_result.IsError());
    EXPECT_NE(plan_result.GetError().GetMessage().find("长度为0"), std::string::npos);
}
