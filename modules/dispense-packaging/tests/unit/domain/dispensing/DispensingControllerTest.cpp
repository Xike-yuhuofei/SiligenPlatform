#include <gtest/gtest.h>

#include "domain/dispensing/domain-services/DispensingController.h"
#include "shared/types/Point.h"

using Siligen::TrajectoryPoint;
using Siligen::Shared::Types::Point2D;
using Siligen::Domain::Dispensing::DomainServices::ControllerConfig;
using Siligen::Domain::Dispensing::DomainServices::DispensingController;

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
    EXPECT_FALSE(output.trigger_positions.empty());
    EXPECT_TRUE(output.interpolation.empty());
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
    EXPECT_FALSE(output.trigger_positions.empty());
    EXPECT_TRUE(output.interpolation.empty());
}
