#include "domain/motion/domain-services/MotionPlanner.h"
#include "motion_planning/contracts/MotionPlanningReport.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "motion_planning/contracts/TimePlanningConfig.h"

#include <gtest/gtest.h>

#include <type_traits>

TEST(MotionPlannerOwnerPathTest, CanonicalTypesStayInMotionPlanning) {
    using TimePlanningConfig = Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using MotionPlanningReport = Siligen::MotionPlanning::Contracts::MotionPlanningReport;
    using MotionTrajectory = Siligen::MotionPlanning::Contracts::MotionTrajectory;

    static_assert(std::is_same_v<decltype(MotionTrajectory{}.planning_report), MotionPlanningReport>);
    EXPECT_TRUE((std::is_same_v<decltype(TimePlanningConfig{}.arc_tolerance_mm), Siligen::Shared::Types::float32>));
}
