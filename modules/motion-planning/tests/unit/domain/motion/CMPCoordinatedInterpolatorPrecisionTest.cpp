#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/domain-services/TrajectoryPlanner.h"

#include <gtest/gtest.h>

#include <cmath>
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
