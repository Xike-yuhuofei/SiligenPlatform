#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/motion/domain-services/MotionPlanner.h"

#include <gtest/gtest.h>

namespace {
constexpr float kEpsilon = 1e-3f;
}

TEST(InterpolationProgramPlannerTest, BuildsLinearProgramFromMotionPlannerTrajectory) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
    using Siligen::Domain::Motion::Ports::InterpolationType;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

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

    TimePlanningConfig cfg;
    cfg.vmax = 100.0f;
    cfg.amax = 200.0f;
    cfg.sample_dt = 0.01f;

    MotionPlanner motion_planner;
    auto trajectory = motion_planner.Plan(path, cfg);

    InterpolationProgramPlanner program_planner;
    auto program_result = program_planner.BuildProgram(path, trajectory, cfg.amax);

    ASSERT_TRUE(program_result.IsSuccess());
    const auto& program = program_result.Value();
    ASSERT_EQ(program.size(), 1u);
    EXPECT_EQ(program[0].type, InterpolationType::LINEAR);
    EXPECT_NEAR(program[0].positions[0], 10.0f, kEpsilon);
    EXPECT_NEAR(program[0].positions[1], 0.0f, kEpsilon);
}

TEST(InterpolationProgramPlannerTest, SplitsFullCircleArcIntoTwoSegments) {
    using Siligen::Shared::Types::Point2D;
    using Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
    using Siligen::Domain::Motion::Ports::InterpolationType;
    using Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.geometry.type = SegmentType::Arc;
    seg.geometry.arc.center = Point2D(0.0f, 0.0f);
    seg.geometry.arc.radius = 5.0f;
    seg.geometry.arc.start_angle_deg = 0.0f;
    seg.geometry.arc.end_angle_deg = 360.0f;
    seg.geometry.arc.clockwise = false;
    seg.geometry.length = 0.0f;
    path.segments.push_back(seg);

    TimePlanningConfig cfg;
    cfg.vmax = 50.0f;
    cfg.amax = 100.0f;
    cfg.sample_dt = 0.01f;

    MotionPlanner motion_planner;
    auto trajectory = motion_planner.Plan(path, cfg);

    InterpolationProgramPlanner program_planner;
    auto program_result = program_planner.BuildProgram(path, trajectory, cfg.amax);

    ASSERT_TRUE(program_result.IsSuccess());
    const auto& program = program_result.Value();
    ASSERT_EQ(program.size(), 2u);
    EXPECT_EQ(program[0].type, InterpolationType::CIRCULAR_CCW);
    EXPECT_EQ(program[1].type, InterpolationType::CIRCULAR_CCW);
}
