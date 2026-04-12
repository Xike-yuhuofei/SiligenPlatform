#include "domain/motion/domain-services/HomingProcess.h"
#include "domain/motion/domain-services/JogController.h"
#include "domain/motion/domain-services/MotionBufferController.h"
#include "domain/motion/domain-services/ReadyZeroDecisionService.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path RepoRoot() {
    fs::path current = fs::path(__FILE__).lexically_normal().parent_path();
    for (int i = 0; i < 6; ++i) {
        current = current.parent_path();
    }
    return current;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    EXPECT_TRUE(input.is_open()) << "failed to open " << path.string();
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::size_t CountOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return 0U;
    }

    std::size_t count = 0U;
    std::size_t pos = haystack.find(needle);
    while (pos != std::string::npos) {
        ++count;
        pos = haystack.find(needle, pos + needle.size());
    }
    return count;
}

TEST(MotionExecutionOwnerBoundaryTest, ExecutionHeadersRemainResolvableFromRuntimeExecutionOwnerSurface) {
    using Siligen::Domain::Motion::MotionBufferController;
    using namespace Siligen::Domain::Motion::DomainServices;

    static_assert(std::is_class_v<HomingProcess>);
    static_assert(std::is_class_v<JogController>);
    static_assert(std::is_class_v<MotionBufferController>);
    static_assert(std::is_class_v<ReadyZeroDecisionService>);

    EXPECT_TRUE((std::is_same_v<
        decltype(HomeAxesResponse{}.axis_results),
        std::vector<HomeAxesResponse::AxisResult>>));
}

TEST(MotionExecutionOwnerBoundaryTest, WorkflowExecutionCompatibilityHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> removed_headers = {{
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/HomingProcess.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/JogController.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionBufferController.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/ReadyZeroDecisionService.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, MotionPlanningExecutionCompatibilityHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> removed_headers = {{
        repo_root / "modules/motion-planning/domain/motion/domain-services/HomingProcess.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/JogController.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionBufferController.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, WorkflowLegacyRuntimeConcreteHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> removed_headers = {{
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, ValidatedInterpolationPortMovesToRuntimeExecutionApplication) {
    const fs::path repo_root = RepoRoot();
    const fs::path legacy_header =
        repo_root / "modules/motion-planning/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h";
    const fs::path legacy_source =
        repo_root / "modules/motion-planning/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.cpp";
    const fs::path runtime_header =
        repo_root / "modules/runtime-execution/application/include/runtime_execution/application/services/motion/interpolation/ValidatedInterpolationPort.h";
    const fs::path runtime_source =
        repo_root / "modules/runtime-execution/application/services/motion/interpolation/ValidatedInterpolationPort.cpp";

    EXPECT_FALSE(fs::exists(legacy_header)) << legacy_header.string();
    EXPECT_FALSE(fs::exists(legacy_source)) << legacy_source.string();
    EXPECT_TRUE(fs::exists(runtime_header)) << runtime_header.string();
    EXPECT_TRUE(fs::exists(runtime_source)) << runtime_source.string();

    const std::string runtime_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/application/CMakeLists.txt");
    const std::string workflow_compat =
        ReadTextFile(repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h");

    EXPECT_NE(runtime_cmake.find("services/motion/interpolation/ValidatedInterpolationPort.cpp"), std::string::npos);
    EXPECT_NE(
        workflow_compat.find("runtime-execution/application/include/runtime_execution/application/services/motion/interpolation/ValidatedInterpolationPort.h"),
        std::string::npos);
}

TEST(MotionExecutionOwnerBoundaryTest, MotionPlanningExecutionImplementationsAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> legacy_sources = {{
        repo_root / "modules/motion-planning/domain/motion/domain-services/HomingProcess.cpp",
        repo_root / "modules/motion-planning/domain/motion/domain-services/JogController.cpp",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionBufferController.cpp",
        repo_root / "modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.cpp",
    }};

    for (const auto& source : legacy_sources) {
        EXPECT_FALSE(fs::exists(source)) << source.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, NonOwnerRuntimeConcreteImplementationsAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> legacy_sources = {{
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.cpp",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.cpp",
    }};

    for (const auto& source : legacy_sources) {
        EXPECT_FALSE(fs::exists(source)) << source.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, WorkflowNoLongerOwnsExecutionConcreteTarget) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/domain/CMakeLists.txt");

    EXPECT_EQ(workflow_cmake.find("add_library(siligen_motion_execution_services STATIC"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("MotionBufferController.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("JogController.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("HomingProcess.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("ReadyZeroDecisionService.cpp"), std::string::npos);
}

TEST(MotionExecutionOwnerBoundaryTest, RuntimeExecutionApplicationTargetOwnsConcreteMotionSources) {
    const fs::path repo_root = RepoRoot();
    const std::string runtime_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/application/CMakeLists.txt");

    EXPECT_NE(runtime_cmake.find("add_library(siligen_runtime_execution_motion_execution_services STATIC"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/execution/MotionBufferController.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/execution/JogController.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/execution/HomingProcess.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/execution/ReadyZeroDecisionService.cpp"), std::string::npos);
    EXPECT_EQ(runtime_cmake.find("modules/workflow/domain/domain/motion/domain-services"), std::string::npos);
    EXPECT_EQ(runtime_cmake.find("siligen_motion_execution_services"), std::string::npos);
}

TEST(MotionExecutionOwnerBoundaryTest, PlanningTargetRetainsPlanningOwnersButNotExecutionSources) {
    const fs::path repo_root = RepoRoot();
    const std::string planning_cmake =
        ReadTextFile(repo_root / "modules/motion-planning/domain/motion/CMakeLists.txt");

    EXPECT_NE(planning_cmake.find("domain-services/TrajectoryPlanner.cpp"), std::string::npos);
    EXPECT_NE(planning_cmake.find("domain-services/TimeTrajectoryPlanner.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/HomingProcess.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/JogController.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/MotionBufferController.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/ReadyZeroDecisionService.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/MotionControlServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(planning_cmake.find("domain-services/MotionStatusServiceImpl.cpp"), std::string::npos);
}

}  // namespace
