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

TEST(MotionExecutionOwnerBoundaryTest, LegacyExecutionHeadersRemainResolvableAndExposeWorkflowLayout) {
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

TEST(MotionExecutionOwnerBoundaryTest, MotionPlanningExecutionHeadersAreThinCompatibilityShims) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 4> expectations = {{
        {repo_root / "modules/motion-planning/domain/motion/domain-services/HomingProcess.h",
         "#include \"../../../../workflow/domain/include/domain/motion/domain-services/HomingProcess.h\""},
        {repo_root / "modules/motion-planning/domain/motion/domain-services/JogController.h",
         "#include \"../../../../workflow/domain/include/domain/motion/domain-services/JogController.h\""},
        {repo_root / "modules/motion-planning/domain/motion/domain-services/MotionBufferController.h",
         "#include \"../../../../workflow/domain/include/domain/motion/domain-services/MotionBufferController.h\""},
        {repo_root / "modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.h",
         "#include \"../../../../workflow/domain/include/domain/motion/domain-services/ReadyZeroDecisionService.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Legacy compatibility shim"), std::string::npos) << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, WorkflowLegacyRuntimeConcreteHeadersRemainThinShims) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 4> expectations = {{
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.h",
         "#include \"../../../include/domain/motion/domain-services/MotionControlServiceImpl.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h",
         "#include \"../../../include/domain/motion/domain-services/MotionStatusServiceImpl.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h",
         "#include \"runtime_execution/application/services/motion/MotionControlServiceImpl.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h",
         "#include \"runtime_execution/application/services/motion/MotionStatusServiceImpl.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Runtime concrete owner lives in runtime-execution"), std::string::npos)
            << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
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

TEST(MotionExecutionOwnerBoundaryTest, WorkflowExecutionTargetOnlyUsesWorkflowOwnerSources) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/domain/CMakeLists.txt");

    EXPECT_NE(workflow_cmake.find("add_library(siligen_motion_execution_services STATIC"), std::string::npos);
    EXPECT_NE(workflow_cmake.find("${SILIGEN_MOTION_EXECUTION_DOMAIN_MOTION_DIR}/domain-services/MotionBufferController.cpp"),
              std::string::npos);
    EXPECT_NE(workflow_cmake.find("${SILIGEN_MOTION_EXECUTION_DOMAIN_MOTION_DIR}/domain-services/JogController.cpp"),
              std::string::npos);
    EXPECT_NE(workflow_cmake.find("${SILIGEN_MOTION_EXECUTION_DOMAIN_MOTION_DIR}/domain-services/HomingProcess.cpp"),
              std::string::npos);
    EXPECT_NE(workflow_cmake.find("${SILIGEN_MOTION_EXECUTION_DOMAIN_MOTION_DIR}/domain-services/ReadyZeroDecisionService.cpp"),
              std::string::npos);
    EXPECT_EQ(workflow_cmake.find("MotionControlServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("MotionStatusServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("motion-planning/domain/motion/domain-services"), std::string::npos);
}

TEST(MotionExecutionOwnerBoundaryTest, RuntimeExecutionApplicationTargetOwnsConcreteMotionSources) {
    const fs::path repo_root = RepoRoot();
    const std::string runtime_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/application/CMakeLists.txt");

    EXPECT_NE(runtime_cmake.find("set_target_properties(siligen_runtime_execution_application PROPERTIES"),
              std::string::npos);
    EXPECT_NE(runtime_cmake.find("SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT \"modules/runtime-execution\""),
              std::string::npos);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "MotionControlServiceImpl.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "MotionStatusServiceImpl.cpp"), 1U);
    EXPECT_NE(runtime_cmake.find("services/motion/MotionControlServiceImpl.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/MotionStatusServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(runtime_cmake.find("domain/motion/domain-services/MotionControlServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(runtime_cmake.find("domain/motion/domain-services/MotionStatusServiceImpl.cpp"), std::string::npos);
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
