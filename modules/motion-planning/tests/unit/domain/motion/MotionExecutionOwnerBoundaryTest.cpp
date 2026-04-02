#include "runtime_execution/application/services/motion/HomingProcess.h"
#include "runtime_execution/application/services/motion/JogController.h"
#include "runtime_execution/application/services/motion/MotionBufferController.h"
#include "runtime_execution/application/services/motion/ReadyZeroDecisionService.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"

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

TEST(MotionExecutionOwnerBoundaryTest, RuntimeExecutionCanonicalHeadersRemainResolvable) {
    using Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse;
    using Siligen::RuntimeExecution::Application::Services::Motion::HomingProcess;
    using Siligen::RuntimeExecution::Application::Services::Motion::JogController;
    using Siligen::RuntimeExecution::Application::Services::Motion::MotionBufferController;
    using Siligen::RuntimeExecution::Application::Services::Motion::ReadyZeroDecisionService;

    static_assert(std::is_class_v<HomingProcess>);
    static_assert(std::is_class_v<JogController>);
    static_assert(std::is_class_v<MotionBufferController>);
    static_assert(std::is_class_v<ReadyZeroDecisionService>);

    EXPECT_TRUE((std::is_same_v<
        decltype(HomeAxesResponse{}.axis_results),
        std::vector<HomeAxesResponse::AxisResult>>));
}

TEST(MotionExecutionOwnerBoundaryTest, MotionPlanningLegacyExecutionHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 6> legacy_headers = {{
        repo_root / "modules/motion-planning/domain/motion/domain-services/HomingProcess.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/JogController.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionBufferController.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/ReadyZeroDecisionService.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionControlServiceImpl.h",
        repo_root / "modules/motion-planning/domain/motion/domain-services/MotionStatusServiceImpl.h",
    }};

    for (const auto& header : legacy_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionExecutionOwnerBoundaryTest, WorkflowLegacyExecutionHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 12> legacy_headers = {{
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/HomingProcess.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/JogController.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionBufferController.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/ReadyZeroDecisionService.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/HomingProcess.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/JogController.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionBufferController.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/ReadyZeroDecisionService.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h",
    }};

    for (const auto& header : legacy_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
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

TEST(MotionExecutionOwnerBoundaryTest, WorkflowNoLongerDefinesMotionExecutionCompatibilityTarget) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/domain/CMakeLists.txt");

    EXPECT_EQ(workflow_cmake.find("add_library(siligen_motion_execution_services STATIC"), std::string::npos);
    EXPECT_NE(workflow_cmake.find("target_link_libraries(siligen_domain_services INTERFACE"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("siligen_motion_execution_services"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("MotionControlServiceImpl.cpp"), std::string::npos);
    EXPECT_EQ(workflow_cmake.find("MotionStatusServiceImpl.cpp"), std::string::npos);
}

TEST(MotionExecutionOwnerBoundaryTest, RuntimeExecutionApplicationTargetOwnsConcreteMotionSources) {
    const fs::path repo_root = RepoRoot();
    const std::string runtime_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/application/CMakeLists.txt");

    EXPECT_NE(runtime_cmake.find("set_target_properties(siligen_runtime_execution_application PROPERTIES"),
              std::string::npos);
    EXPECT_NE(runtime_cmake.find("add_library(siligen_runtime_execution_motion_services STATIC"),
              std::string::npos);
    EXPECT_NE(runtime_cmake.find("SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT \"modules/runtime-execution\""),
              std::string::npos);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "MotionBufferController.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "JogController.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "HomingProcess.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "ReadyZeroDecisionService.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "MotionControlServiceImpl.cpp"), 1U);
    EXPECT_EQ(CountOccurrences(runtime_cmake, "MotionStatusServiceImpl.cpp"), 1U);
    EXPECT_NE(runtime_cmake.find("services/motion/MotionBufferController.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/JogController.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/HomingProcess.cpp"), std::string::npos);
    EXPECT_NE(runtime_cmake.find("services/motion/ReadyZeroDecisionService.cpp"), std::string::npos);
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

TEST(MotionExecutionOwnerBoundaryTest, WorkflowDomainRootNoLongerLinksMachineCompatibilityTarget) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_domain_root =
        ReadTextFile(repo_root / "modules/workflow/domain/CMakeLists.txt");
    const std::string workflow_machine_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/domain/machine/CMakeLists.txt");

    EXPECT_EQ(workflow_domain_root.find("domain_machine"), std::string::npos);
    EXPECT_EQ(workflow_machine_cmake.find("add_library(domain_machine STATIC"), std::string::npos);
}

}  // namespace
