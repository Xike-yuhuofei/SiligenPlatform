#include "domain/motion/domain-services/MotionPlanner.h"
#include "domain/trajectory/domain-services/MotionPlanner.h"

#include <gtest/gtest.h>

#include <type_traits>

TEST(MotionPlannerOwnerPathTest, LegacyHeaderAliasesCanonicalType) {
    using CanonicalPlanner = Siligen::Domain::Motion::DomainServices::MotionPlanner;
    using LegacyPlanner = Siligen::Domain::Trajectory::DomainServices::MotionPlanner;

    static_assert(std::is_same_v<CanonicalPlanner, LegacyPlanner>);
    EXPECT_TRUE((std::is_same_v<CanonicalPlanner, LegacyPlanner>));
}
