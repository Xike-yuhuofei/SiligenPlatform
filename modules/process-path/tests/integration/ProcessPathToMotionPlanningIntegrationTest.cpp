#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "support/ProcessPathTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using Siligen::Application::Services::MotionPlanning::MotionPlanningFacade;
using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Tests::Support::MakeLeadInLeadOutRequest;
using Siligen::ProcessPath::Tests::Support::MakeMotionPlanningConfig;

TEST(ProcessPathToMotionPlanningIntegrationTest, PlansMotionTrajectoryFromShapedProcessPath) {
    ProcessPathFacade process_path_facade;
    const auto build_result = process_path_facade.Build(MakeLeadInLeadOutRequest());

    ASSERT_EQ(build_result.shaped_path.segments.size(), 3u);

    MotionPlanningFacade motion_planning_facade;
    const auto motion_plan = motion_planning_facade.Plan(build_result.shaped_path, MakeMotionPlanningConfig());

    ASSERT_FALSE(motion_plan.points.empty());
    EXPECT_GT(motion_plan.total_time, 0.0f);
    EXPECT_GT(motion_plan.total_length, 10.0f);
    EXPECT_NEAR(motion_plan.points.front().position.x, -1.0f, 1e-3f);
    EXPECT_NEAR(motion_plan.points.back().position.x, 10.0f, 1e-3f);
    EXPECT_EQ(motion_plan.points.front().process_tag, ProcessTag::Start);
    EXPECT_EQ(motion_plan.points.back().process_tag, ProcessTag::End);
    EXPECT_TRUE(std::any_of(motion_plan.points.begin(), motion_plan.points.end(), [](const auto& point) {
        return !point.dispense_on;
    }));
}

}  // namespace
