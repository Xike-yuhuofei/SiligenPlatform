#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"

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

TEST(NoRuntimeControlLeakTest, LegacyRuntimePortHeadersAreRemovedFromMotionPlanningOwner) {
    const auto repo_root = RepoRoot();
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/ports/IIOControlPort.h"));
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h"));
    EXPECT_FALSE(std::filesystem::exists(repo_root / "modules/motion-planning/domain/motion/ports/IInterpolationPort.h"));
}

TEST(NoRuntimeControlLeakTest, RuntimeContractsRemainCanonicalPublicOwner) {
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IIOControlPort,
        Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>));
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IOStatus,
        Siligen::RuntimeExecution::Contracts::Motion::IOStatus>));
    EXPECT_TRUE((std::is_same_v<
        Siligen::Domain::Motion::Ports::IMotionRuntimePort,
        Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>));
    EXPECT_TRUE((std::is_abstract_v<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>));
}

}  // namespace
