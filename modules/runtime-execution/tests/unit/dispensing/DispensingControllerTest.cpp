#include <gtest/gtest.h>

#include "services/dispensing/DispensingController.h"
#include "shared/types/Point.h"

using Siligen::MotionPlanning::Contracts::MotionTrajectory;
using Siligen::MotionPlanning::Contracts::MotionTrajectoryPoint;
using Siligen::Point3D;
using Siligen::RuntimeExecution::Application::Services::Dispensing::ControllerConfig;
using Siligen::RuntimeExecution::Application::Services::Dispensing::DispensingController;
using Siligen::Shared::Types::Point2D;
using Siligen::TrajectoryPoint;

TEST(DispensingControllerTest, MonotonicTriggerUsesHardware) {
    DispensingController controller;
    ControllerConfig config;
    config.use_hardware_trigger = true;
    config.spatial_interval_mm = 5.0f;
    config.pulse_per_mm = 100.0f;

    TrajectoryPoint p0(Point2D(0.0f, 0.0f), 10.0f);
    p0.timestamp = 0.0f;
    TrajectoryPoint p1(Point2D(10.0f, 0.0f), 10.0f);
    p1.timestamp = 1.0f;
    TrajectoryPoint p2(Point2D(20.0f, 0.0f), 10.0f);
    p2.timestamp = 2.0f;

    std::vector<TrajectoryPoint> trajectory{p0, p1, p2};

    auto result = controller.Build(trajectory, config);
    ASSERT_TRUE(result.IsSuccess());

    const auto output = result.Value();
    EXPECT_TRUE(output.use_hardware_trigger);
    EXPECT_FALSE(output.trigger_events.empty());
    EXPECT_TRUE(output.interpolation.empty());
    EXPECT_TRUE(output.trigger_events.front().sequence_index == 0U);
    EXPECT_TRUE(output.trigger_events.front().path_position_mm >= 0.0f);
}

TEST(DispensingControllerTest, NonMonotonicTriggerUsesHardware) {
    DispensingController controller;
    ControllerConfig config;
    config.use_hardware_trigger = true;
    config.spatial_interval_mm = 5.0f;
    config.pulse_per_mm = 100.0f;

    TrajectoryPoint p0(Point2D(0.0f, 0.0f), 10.0f);
    p0.timestamp = 0.0f;
    TrajectoryPoint p1(Point2D(10.0f, 0.0f), 10.0f);
    p1.timestamp = 1.0f;
    TrajectoryPoint p2(Point2D(0.0f, 0.0f), 10.0f);
    p2.timestamp = 2.0f;

    std::vector<TrajectoryPoint> trajectory{p0, p1, p2};

    auto result = controller.Build(trajectory, config);
    ASSERT_TRUE(result.IsSuccess());

    const auto output = result.Value();
    EXPECT_TRUE(output.use_hardware_trigger);
    EXPECT_FALSE(output.trigger_events.empty());
    EXPECT_TRUE(output.interpolation.empty());
    EXPECT_TRUE(output.trigger_events.front().sequence_index == 0U);
    EXPECT_TRUE(output.trigger_events.front().path_position_mm >= 0.0f);
}

TEST(DispensingControllerTest, MotionTrajectoryTriggerEventsStayMonotonicAcrossSeparatedDispenseSegments) {
    DispensingController controller;
    ControllerConfig config;
    config.use_hardware_trigger = true;
    config.spatial_interval_mm = 5.0f;
    config.pulse_per_mm = 100.0f;

    MotionTrajectory trajectory;

    MotionTrajectoryPoint p0;
    p0.t = 0.0f;
    p0.position = Point3D(0.0f, 0.0f, 0.0f);
    p0.dispense_on = true;

    MotionTrajectoryPoint p1;
    p1.t = 1.0f;
    p1.position = Point3D(10.0f, 0.0f, 0.0f);
    p1.dispense_on = true;

    MotionTrajectoryPoint p2;
    p2.t = 2.0f;
    p2.position = Point3D(20.0f, 0.0f, 0.0f);
    p2.dispense_on = false;

    MotionTrajectoryPoint p3;
    p3.t = 3.0f;
    p3.position = Point3D(30.0f, 0.0f, 0.0f);
    p3.dispense_on = true;

    MotionTrajectoryPoint p4;
    p4.t = 4.0f;
    p4.position = Point3D(40.0f, 0.0f, 0.0f);
    p4.dispense_on = true;

    trajectory.points = {p0, p1, p2, p3, p4};
    trajectory.total_time = 4.0f;

    auto result = controller.Build(trajectory, config);
    ASSERT_TRUE(result.IsSuccess());

    const auto output = result.Value();
    ASSERT_EQ(output.trigger_events.size(), 4U);
    EXPECT_TRUE(output.use_hardware_trigger);
    EXPECT_TRUE(output.interpolation.empty());
    EXPECT_FLOAT_EQ(output.trigger_events[0].path_position_mm, 5.0f);
    EXPECT_FLOAT_EQ(output.trigger_events[1].path_position_mm, 10.0f);
    EXPECT_FLOAT_EQ(output.trigger_events[2].path_position_mm, 35.0f);
    EXPECT_FLOAT_EQ(output.trigger_events[3].path_position_mm, 40.0f);

    for (size_t index = 1; index < output.trigger_events.size(); ++index) {
        EXPECT_GE(output.trigger_events[index].path_position_mm,
                  output.trigger_events[index - 1].path_position_mm);
    }
}
