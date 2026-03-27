#include "application/services/motion_planning/MotionPlanningFacade.h"

#include <gtest/gtest.h>

TEST(MotionPlanningFacadeTest, PlansTrajectoryFromProcessPath) {
    using Siligen::Application::Services::MotionPlanning::MotionPlanningFacade;
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
    using Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
    using Siligen::Domain::Trajectory::ValueObjects::SegmentType;

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

    MotionConfig config;
    config.vmax = 50.0f;
    config.amax = 200.0f;
    config.sample_dt = 0.01f;

    MotionPlanningFacade facade;
    const auto trajectory = facade.Plan(path, config);

    ASSERT_FALSE(trajectory.points.empty());
    EXPECT_GT(trajectory.total_time, 0.0f);
}
