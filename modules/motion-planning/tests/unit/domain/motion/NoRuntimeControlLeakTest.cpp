#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>

namespace {

namespace fs = std::filesystem;

fs::path RepoRoot() {
    fs::path current = fs::path(__FILE__).lexically_normal().parent_path();
    for (int i = 0; i < 6; ++i) {
        current = current.parent_path();
    }
    return current;
}

TEST(NoRuntimeControlLeakTest, CanonicalRuntimePortContractsRemainResolvable) {
    EXPECT_TRUE((std::is_abstract_v<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>));
    EXPECT_TRUE((std::is_abstract_v<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>));
}

TEST(NoRuntimeControlLeakTest, MotionPlanningLegacyRuntimePortHeadersAreRemoved) {
    const auto root = RepoRoot();
    const std::array<fs::path, 2> legacy_headers = {{
        root / "modules/motion-planning/domain/motion/ports/IIOControlPort.h",
        root / "modules/motion-planning/domain/motion/ports/IMotionRuntimePort.h",
    }};

    for (const auto& header : legacy_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(NoRuntimeControlLeakTest, WorkflowLegacyRuntimePortHeadersAreRemoved) {
    const auto root = RepoRoot();
    const std::array<fs::path, 4> legacy_headers = {{
        root / "modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h",
        root / "modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h",
        root / "modules/workflow/domain/domain/motion/ports/IIOControlPort.h",
        root / "modules/workflow/domain/domain/motion/ports/IMotionRuntimePort.h",
    }};

    for (const auto& header : legacy_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

}  // namespace
