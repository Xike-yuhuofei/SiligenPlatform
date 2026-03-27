#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/motion/value-objects/MotionPlanningReport.h"
#include "domain/motion/value-objects/MotionTrajectory.h"
#include "domain/motion/value-objects/TimePlanningConfig.h"

#include <gtest/gtest.h>

#include <type_traits>

TEST(MotionPlannerOwnerPathTest, CanonicalTypesStayInMotionPlanning) {
    using TimePlanningConfig = Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
    using MotionPlanningReport = Siligen::Domain::Motion::ValueObjects::MotionPlanningReport;
    using MotionTrajectory = Siligen::Domain::Motion::ValueObjects::MotionTrajectory;

    static_assert(std::is_same_v<decltype(MotionTrajectory{}.planning_report), MotionPlanningReport>);
    EXPECT_TRUE((std::is_same_v<decltype(TimePlanningConfig{}.arc_tolerance_mm), Siligen::Shared::Types::float32>));
}
