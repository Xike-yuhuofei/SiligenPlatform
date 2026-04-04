#include "application/services/motion_planning/CmpInterpolationFacade.h"
#include "application/services/motion_planning/InterpolationProgramFacade.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/motion_planning/TrajectoryInterpolationFacade.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace {

constexpr float kEpsilon = 1e-4f;
constexpr float kPi = 3.14159265358979323846f;

float ComputeArcLength(const Siligen::ProcessPath::Contracts::ArcPrimitive& arc) {
    float sweep_deg = arc.end_angle_deg - arc.start_angle_deg;
    if (arc.clockwise && sweep_deg > 0.0f) {
        sweep_deg -= 360.0f;
    } else if (!arc.clockwise && sweep_deg < 0.0f) {
        sweep_deg += 360.0f;
    }
    return std::abs(sweep_deg) * kPi / 180.0f * arc.radius;
}

Siligen::Shared::Types::Point2D ComputeArcPoint(
    const Siligen::ProcessPath::Contracts::ArcPrimitive& arc,
    float angle_deg) {
    const float angle_rad = angle_deg * kPi / 180.0f;
    return Siligen::Shared::Types::Point2D(
        arc.center.x + arc.radius * std::cos(angle_rad),
        arc.center.y + arc.radius * std::sin(angle_rad));
}

Siligen::ProcessPath::Contracts::ProcessPath BuildLineProcessPath() {
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;
    using Siligen::Shared::Types::Point2D;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.geometry.type = SegmentType::Line;
    seg.geometry.line.start = Point2D(0.0f, 0.0f);
    seg.geometry.line.end = Point2D(10.0f, 0.0f);
    seg.geometry.length = 10.0f;
    path.segments.push_back(seg);
    return path;
}

Siligen::MotionPlanning::Contracts::TimePlanningConfig BuildTimePlanningConfig() {
    Siligen::MotionPlanning::Contracts::TimePlanningConfig config;
    config.vmax = 50.0f;
    config.amax = 200.0f;
    config.sample_dt = 0.01f;
    return config;
}

}  // namespace

TEST(MotionPlanningPublicSurfaceContractTest, TrajectoryInterpolationFacadeUsesContractAlgorithmEnum) {
    using Siligen::Application::Services::MotionPlanning::TrajectoryInterpolationFacade;
    using Siligen::MotionPlanning::Contracts::InterpolationAlgorithm;
    using Siligen::Point2D;

    std::vector<Point2D> points = {
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
    };

    Siligen::InterpolationConfig config;
    config.max_velocity = 20.0f;
    config.max_acceleration = 100.0f;
    config.max_jerk = 500.0f;
    config.time_step = 0.01f;

    TrajectoryInterpolationFacade facade;
    const auto result = facade.Interpolate(points, InterpolationAlgorithm::LINEAR, config);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    ASSERT_GE(result.Value().size(), 2U);
    EXPECT_NEAR(result.Value().front().position.x, 0.0f, kEpsilon);
    EXPECT_TRUE(std::any_of(
        result.Value().begin(),
        result.Value().end(),
        [](const auto& point) { return point.position.x > 0.0f; }));
    for (const auto& point : result.Value()) {
        EXPECT_GE(point.position.x, -kEpsilon);
        EXPECT_LE(point.position.x, 10.0f + kEpsilon);
        EXPECT_NEAR(point.position.y, 0.0f, kEpsilon);
    }

    const auto optimized =
        facade.OptimizeTrajectoryDensity(result.Value(), InterpolationAlgorithm::LINEAR, 5.0f);
    EXPECT_FALSE(optimized.empty());
}

TEST(MotionPlanningPublicSurfaceContractTest, InterpolationProgramFacadeBuildsProgramFromContractPathAndTrajectory) {
    using Siligen::Application::Services::MotionPlanning::InterpolationProgramFacade;
    using Siligen::Application::Services::MotionPlanning::MotionPlanningFacade;

    const auto path = BuildLineProcessPath();
    const auto config = BuildTimePlanningConfig();

    MotionPlanningFacade planning_facade;
    const auto trajectory = planning_facade.Plan(path, config);
    ASSERT_FALSE(trajectory.points.empty());

    InterpolationProgramFacade interpolation_program_facade;
    const auto result = interpolation_program_facade.BuildProgram(path, trajectory, config.amax);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_FALSE(result.Value().empty());
}

TEST(MotionPlanningPublicSurfaceContractTest, CmpInterpolationFacadeAcceptsContractProcessPathAndExternalTriggers) {
    using Siligen::Application::Services::MotionPlanning::CmpInterpolationFacade;
    using Siligen::CMPConfiguration;
    using Siligen::CMPTriggerMode;
    using Siligen::DispensingTriggerPoint;
    using Siligen::InterpolationConfig;
    using Siligen::ProcessPath::Contracts::ArcPrimitive;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;
    using Siligen::Shared::Types::Point2D;

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
    seg.geometry.length = ComputeArcLength(arc);
    path.segments.push_back(seg);

    DispensingTriggerPoint trigger;
    trigger.position = ComputeArcPoint(arc, 45.0f);
    trigger.trigger_distance = ComputeArcLength(arc) * 0.5f;
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

    InterpolationConfig config;
    config.max_velocity = 20.0f;
    config.max_acceleration = 100.0f;
    config.max_jerk = 500.0f;
    config.time_step = 0.01f;
    config.position_tolerance = 0.01f;

    CmpInterpolationFacade facade;
    const auto result = facade.PositionTriggeredDispensing(path, {trigger}, cmp_config, config);

    ASSERT_FALSE(result.empty());
    auto trigger_it = std::find_if(
        result.begin(),
        result.end(),
        [](const auto& point) { return point.enable_position_trigger; });
    ASSERT_NE(trigger_it, result.end());
    EXPECT_NEAR(trigger_it->position.x, trigger.position.x, kEpsilon);
    EXPECT_NEAR(trigger_it->position.y, trigger.position.y, kEpsilon);
}
