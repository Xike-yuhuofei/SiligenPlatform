#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path RepoRoot() {
    auto path = std::filesystem::absolute(__FILE__);
    for (int i = 0; i < 6; ++i) {
        path = path.parent_path();
    }
    return path;
}

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

}  // namespace

TEST(WorkflowBoundaryCutoverTest, MotionRuntimeAssemblyArtifactsArePhysicallyRemoved) {
    const auto root = RepoRoot();

    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.cpp"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/tests/process-runtime-core/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp"));
}

TEST(WorkflowBoundaryCutoverTest, WorkflowLegacyRuntimeConcreteShimsArePhysicallyRemoved) {
    const auto root = RepoRoot();

    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h"));
}

TEST(WorkflowBoundaryCutoverTest, WorkflowLegacyRuntimePortAliasHeadersArePhysicallyRemoved) {
    const auto root = RepoRoot();

    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/domain/motion/ports/IIOControlPort.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/domain/domain/motion/ports/IMotionRuntimePort.h"));
}

TEST(WorkflowBoundaryCutoverTest, WorkflowMotionConsumersDependOnCanonicalRuntimeContracts) {
    const auto root = RepoRoot();
    const auto initialization_header =
        root / "modules/workflow/application/include/application/usecases/motion/initialization/MotionInitializationUseCase.h";
    const auto coordination_header =
        root / "modules/workflow/application/include/application/usecases/motion/coordination/MotionCoordinationUseCase.h";
    const auto provider_header =
        root / "modules/workflow/application/include/workflow/application/services/motion/runtime/IMotionRuntimeServicesProvider.h";

    ASSERT_TRUE(std::filesystem::exists(initialization_header));
    ASSERT_TRUE(std::filesystem::exists(coordination_header));
    ASSERT_TRUE(std::filesystem::exists(provider_header));

    const auto initialization_text = ReadText(initialization_header);
    const auto coordination_text = ReadText(coordination_header);
    const auto provider_text = ReadText(provider_header);

    EXPECT_NE(initialization_text.find("runtime_execution/contracts/motion/IIOControlPort.h"), std::string::npos);
    EXPECT_EQ(initialization_text.find("domain/motion/ports/IIOControlPort.h"), std::string::npos);

    EXPECT_NE(coordination_text.find("runtime_execution/contracts/motion/IIOControlPort.h"), std::string::npos);
    EXPECT_EQ(coordination_text.find("domain/motion/ports/IIOControlPort.h"), std::string::npos);

    EXPECT_NE(provider_text.find("runtime_execution/contracts/motion/IMotionRuntimePort.h"), std::string::npos);
    EXPECT_EQ(provider_text.find("domain/motion/ports/IMotionRuntimePort.h"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, InitializeSystemUseCaseDependsOnRuntimeContractPortOnly) {
    const auto root = RepoRoot();
    const auto header_path =
        root / "modules/workflow/application/include/application/usecases/system/InitializeSystemUseCase.h";
    const auto source_path =
        root / "modules/workflow/application/usecases/system/InitializeSystemUseCase.cpp";

    ASSERT_TRUE(std::filesystem::exists(header_path));
    ASSERT_TRUE(std::filesystem::exists(source_path));

    const auto header_text = ReadText(header_path);
    const auto source_text = ReadText(source_path);

    EXPECT_NE(header_text.find("IHomeAxesExecutionPort"), std::string::npos);
    EXPECT_NE(source_text.find("IHomeAxesExecutionPort"), std::string::npos);
    EXPECT_EQ(
        header_text.find("Application::UseCases::Motion::Homing::HomeAxesUseCase"),
        std::string::npos);
    EXPECT_EQ(
        source_text.find("runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"),
        std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowBuildGraphNoLongerLinksRuntimeExecutionApplicationPublic) {
    const auto root = RepoRoot();
    const auto workflow_application_cmake = root / "modules/workflow/application/CMakeLists.txt";
    const auto workflow_tests_cmake = root / "modules/workflow/tests/unit/CMakeLists.txt";

    ASSERT_TRUE(std::filesystem::exists(workflow_application_cmake));
    ASSERT_TRUE(std::filesystem::exists(workflow_tests_cmake));

    const auto workflow_application_text = ReadText(workflow_application_cmake);
    const auto workflow_tests_text = ReadText(workflow_tests_cmake);

    EXPECT_EQ(workflow_application_text.find("siligen_runtime_execution_application_public"), std::string::npos);
    EXPECT_EQ(workflow_application_text.find("runtime-execution/application"), std::string::npos);
    EXPECT_EQ(workflow_application_text.find("MotionRuntimeAssemblyFactory.cpp"), std::string::npos);

    EXPECT_EQ(workflow_tests_text.find("siligen_runtime_execution_application_public"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("modules/runtime-execution/application/include"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("modules/runtime-execution/adapters/device/include"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("process_runtime_core_"), std::string::npos);
    EXPECT_EQ(workflow_application_text.find("PROCESS_RUNTIME_CORE_"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowOwnedTestsDoNotCarryRuntimeOwnedMotionSources) {
    const auto root = RepoRoot();
    const auto workflow_tests_cmake = root / "modules/workflow/tests/unit/CMakeLists.txt";

    ASSERT_TRUE(std::filesystem::exists(workflow_tests_cmake));

    const auto workflow_tests_text = ReadText(workflow_tests_cmake);
    EXPECT_EQ(workflow_tests_text.find("JogControllerTest.cpp"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("MotionBufferControllerTest.cpp"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("HomeAxesUseCaseTest.cpp"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("EnsureAxesReadyZeroUseCaseTest.cpp"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("MotionMonitoringUseCaseTest.cpp"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("HomingSupportTest.cpp"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, LegacyProcessRuntimeCoreTestsRootIsRemoved) {
    const auto root = RepoRoot();
    EXPECT_FALSE(std::filesystem::exists(root / "modules/workflow/tests/process-runtime-core"));
}
