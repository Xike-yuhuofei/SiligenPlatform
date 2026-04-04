#include "domain/motion/domain-services/MotionPlanner.h"
#include "motion_planning/contracts/MotionPlanningReport.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "motion_planning/contracts/TimePlanningConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <type_traits>

namespace {

std::filesystem::path RepoRoot() {
    auto current = std::filesystem::path(__FILE__).lexically_normal().parent_path();
    for (int i = 0; i < 6; ++i) {
        current = current.parent_path();
    }
    return current;
}

TEST(MotionPlannerOwnerPathTest, CanonicalTypesStayInMotionPlanning) {
    using TimePlanningConfig = Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using MotionPlanningReport = Siligen::MotionPlanning::Contracts::MotionPlanningReport;
    using MotionTrajectory = Siligen::MotionPlanning::Contracts::MotionTrajectory;

    static_assert(std::is_same_v<decltype(MotionTrajectory{}.planning_report), MotionPlanningReport>);
    EXPECT_TRUE((std::is_same_v<decltype(TimePlanningConfig{}.arc_tolerance_mm), Siligen::Shared::Types::float32>));
}

TEST(MotionPlannerOwnerPathTest, LegacyValueObjectHeadersAreRemovedFromMotionPlanningOwner) {
    const auto repo_root = RepoRoot();
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/value-objects/MotionTrajectory.h"));
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/value-objects/MotionPlanningReport.h"));
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/value-objects/TimePlanningConfig.h"));
}

}  // namespace
