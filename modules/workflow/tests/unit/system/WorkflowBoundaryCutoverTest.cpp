#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
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

std::string ExtractBlock(const std::string& text, const char* pattern) {
    const std::regex regex(pattern, std::regex::ECMAScript);
    const std::smatch match{};
    std::smatch captures;
    if (!std::regex_search(text, captures, regex) || captures.size() < 2) {
        return {};
    }
    return captures[1].str();
}

}  // namespace

TEST(WorkflowBoundaryCutoverTest, MotionRuntimeAssemblyArtifactsRemainWorkflowOwnedBridgeAssets) {
    const auto root = RepoRoot();
    const auto header_path =
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h";
    const auto source_path =
        root / "modules/workflow/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.cpp";

    EXPECT_TRUE(std::filesystem::exists(header_path));
    EXPECT_TRUE(std::filesystem::exists(source_path));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/tests/process-runtime-core/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp"));

    const auto header_text = ReadText(header_path);
    const auto source_text = ReadText(source_path);

    EXPECT_NE(header_text.find("runtime_execution/contracts/motion/IMotionRuntimePort.h"), std::string::npos);
    EXPECT_NE(header_text.find("runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h"),
              std::string::npos);
    EXPECT_NE(source_text.find("std::make_unique<Homing::HomeAxesUseCase>"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowRuntimeConcreteCompatibilityShimsForwardToRuntimeExecutionOwner) {
    const auto root = RepoRoot();
    const auto public_control =
        root / "modules/workflow/domain/include/domain/motion/domain-services/MotionControlServiceImpl.h";
    const auto public_status =
        root / "modules/workflow/domain/include/domain/motion/domain-services/MotionStatusServiceImpl.h";
    const auto private_control =
        root / "modules/workflow/domain/domain/motion/domain-services/MotionControlServiceImpl.h";
    const auto private_status =
        root / "modules/workflow/domain/domain/motion/domain-services/MotionStatusServiceImpl.h";

    ASSERT_TRUE(std::filesystem::exists(public_control));
    ASSERT_TRUE(std::filesystem::exists(public_status));
    ASSERT_TRUE(std::filesystem::exists(private_control));
    ASSERT_TRUE(std::filesystem::exists(private_status));

    EXPECT_NE(ReadText(public_control).find("runtime_execution/application/services/motion/MotionControlServiceImpl.h"),
              std::string::npos);
    EXPECT_NE(ReadText(public_status).find("runtime_execution/application/services/motion/MotionStatusServiceImpl.h"),
              std::string::npos);
    EXPECT_NE(ReadText(private_control).find("../../../include/domain/motion/domain-services/MotionControlServiceImpl.h"),
              std::string::npos);
    EXPECT_NE(ReadText(private_status).find("../../../include/domain/motion/domain-services/MotionStatusServiceImpl.h"),
              std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowRuntimePortCompatibilityHeadersForwardToCanonicalContracts) {
    const auto root = RepoRoot();
    const auto public_io = root / "modules/workflow/domain/include/domain/motion/ports/IIOControlPort.h";
    const auto public_runtime = root / "modules/workflow/domain/include/domain/motion/ports/IMotionRuntimePort.h";
    const auto private_io = root / "modules/workflow/domain/domain/motion/ports/IIOControlPort.h";
    const auto private_runtime = root / "modules/workflow/domain/domain/motion/ports/IMotionRuntimePort.h";

    ASSERT_TRUE(std::filesystem::exists(public_io));
    ASSERT_TRUE(std::filesystem::exists(public_runtime));
    ASSERT_TRUE(std::filesystem::exists(private_io));
    ASSERT_TRUE(std::filesystem::exists(private_runtime));

    EXPECT_NE(ReadText(public_io).find("runtime_execution/contracts/motion/IIOControlPort.h"), std::string::npos);
    EXPECT_NE(ReadText(public_runtime).find("runtime_execution/contracts/motion/IMotionRuntimePort.h"), std::string::npos);
    EXPECT_NE(ReadText(private_io).find("../../../include/domain/motion/ports/IIOControlPort.h"), std::string::npos);
    EXPECT_NE(ReadText(private_runtime).find("../../../include/domain/motion/ports/IMotionRuntimePort.h"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowMotionConsumersUseWorkflowOwnedCompatibilityHeaders) {
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

    EXPECT_NE(initialization_text.find("domain/motion/ports/IIOControlPort.h"), std::string::npos);
    EXPECT_EQ(initialization_text.find("runtime_execution/contracts/motion/IIOControlPort.h"), std::string::npos);

    EXPECT_NE(coordination_text.find("domain/motion/ports/IIOControlPort.h"), std::string::npos);
    EXPECT_EQ(coordination_text.find("runtime_execution/contracts/motion/IIOControlPort.h"), std::string::npos);

    EXPECT_NE(provider_text.find("runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h"),
              std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, InitializeSystemUseCaseDependsOnWorkflowHomeAxesUseCase) {
    const auto root = RepoRoot();
    const auto header_path =
        root / "modules/workflow/application/include/application/usecases/system/InitializeSystemUseCase.h";
    const auto source_path =
        root / "modules/workflow/application/usecases/system/InitializeSystemUseCase.cpp";

    ASSERT_TRUE(std::filesystem::exists(header_path));
    ASSERT_TRUE(std::filesystem::exists(source_path));

    const auto header_text = ReadText(header_path);
    const auto source_text = ReadText(source_path);

    EXPECT_NE(header_text.find("Application::UseCases::Motion::Homing::HomeAxesUseCase"), std::string::npos);
    EXPECT_NE(source_text.find("application/usecases/motion/homing/HomeAxesUseCase.h"), std::string::npos);
    EXPECT_EQ(header_text.find("IHomeAxesExecutionPort"), std::string::npos);
    EXPECT_EQ(source_text.find("IHomeAxesExecutionPort"), std::string::npos);
}

TEST(WorkflowBoundaryCutoverTest, WorkflowBuildGraphKeepsCurrentBridgeBoundary) {
    const auto root = RepoRoot();
    const auto workflow_application_cmake = root / "modules/workflow/application/CMakeLists.txt";
    const auto workflow_tests_cmake = root / "modules/workflow/tests/unit/CMakeLists.txt";

    ASSERT_TRUE(std::filesystem::exists(workflow_application_cmake));
    ASSERT_TRUE(std::filesystem::exists(workflow_tests_cmake));

    const auto workflow_application_text = ReadText(workflow_application_cmake);
    const auto workflow_tests_text = ReadText(workflow_tests_cmake);

    const auto headers_block = ExtractBlock(
        workflow_application_text,
        R"(target_link_libraries\(siligen_workflow_application_headers INTERFACE([\s\S]*?)target_compile_features\(siligen_workflow_application_headers)");
    const auto motion_block = ExtractBlock(
        workflow_application_text,
        R"(target_link_libraries\(siligen_application_motion PUBLIC([\s\S]*?)target_compile_features\(siligen_application_motion)");

    ASSERT_FALSE(headers_block.empty());
    ASSERT_FALSE(motion_block.empty());

    EXPECT_NE(workflow_application_text.find("siligen_runtime_execution_runtime_contracts"), std::string::npos);
    EXPECT_NE(workflow_application_text.find("MotionRuntimeAssemblyFactory.cpp"), std::string::npos);
    EXPECT_EQ(headers_block.find("siligen_runtime_execution_application_public"), std::string::npos);
    EXPECT_EQ(motion_block.find("siligen_runtime_execution_application_public"), std::string::npos);

    EXPECT_EQ(workflow_tests_text.find("process_runtime_core_"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("modules/runtime-execution/application/include"), std::string::npos);
    EXPECT_EQ(workflow_tests_text.find("modules/runtime-execution/adapters/device/include"), std::string::npos);
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
