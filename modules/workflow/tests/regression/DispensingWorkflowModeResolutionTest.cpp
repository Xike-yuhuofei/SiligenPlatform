#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path RepoRoot() {
    auto path = std::filesystem::absolute(__FILE__);
    for (int i = 0; i < 5; ++i) {
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

TEST(DispensingWorkflowModeResolutionTest, LegacyExecuteWorkflowSourcesArePhysicallyRemoved) {
    const auto root = RepoRoot();

    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/usecases/dispensing/DispensingExecutionWorkflowUseCase.cpp"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/include/application/usecases/dispensing/DispensingExecutionWorkflowUseCase.h"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/services/dispensing/DispensingExecutionCompatibilityService.cpp"));
    EXPECT_FALSE(std::filesystem::exists(
        root / "modules/workflow/application/services/dispensing/DispensingExecutionCompatibilityService.h"));
}

TEST(DispensingWorkflowModeResolutionTest, LiveWiringDoesNotReferenceLegacyExecuteAdapters) {
    const auto root = RepoRoot();
    const auto container_path = root / "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp";
    const auto facade_header_path = root / "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h";
    const auto builder_header_path = root / "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h";
    const auto runtime_execution_header_path =
        root / "modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h";

    ASSERT_TRUE(std::filesystem::exists(container_path));
    ASSERT_TRUE(std::filesystem::exists(facade_header_path));
    ASSERT_TRUE(std::filesystem::exists(builder_header_path));
    ASSERT_TRUE(std::filesystem::exists(runtime_execution_header_path));

    const auto container_source = ReadText(container_path);
    const auto facade_header = ReadText(facade_header_path);
    const auto builder_header = ReadText(builder_header_path);
    const auto runtime_execution_header = ReadText(runtime_execution_header_path);

    EXPECT_EQ(container_source.find("DispensingExecutionWorkflowUseCase"), std::string::npos);
    EXPECT_EQ(container_source.find("DispensingExecutionCompatibilityService"), std::string::npos);
    EXPECT_EQ(container_source.find("SetLegacyExecutionForwarders("), std::string::npos);

    EXPECT_EQ(facade_header.find("DispensingExecutionWorkflowUseCase"), std::string::npos);
    EXPECT_EQ(facade_header.find("ExecuteDxfAsync("), std::string::npos);
    EXPECT_EQ(facade_header.find("GetDxfTaskStatus("), std::string::npos);
    EXPECT_EQ(facade_header.find("PauseDxfTask("), std::string::npos);
    EXPECT_EQ(facade_header.find("ResumeDxfTask("), std::string::npos);
    EXPECT_EQ(facade_header.find("CancelDxfTask("), std::string::npos);

    EXPECT_EQ(builder_header.find("DispensingExecutionWorkflowUseCase"), std::string::npos);

    EXPECT_EQ(runtime_execution_header.find("SetLegacyExecutionForwarders("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("LegacyExecuteFn"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("LegacyExecuteAsyncFn"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("DispensingMVPRequest"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("DispensingMVPResult"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("TaskID"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("TaskState"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("TaskExecutionContext"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("TaskStatusResponse"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("Execute(const DispensingMVPRequest& request)"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("ExecuteAsync(const DispensingMVPRequest& request)"), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("ExecuteAsync("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("GetTaskStatus("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("PauseTask("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("ResumeTask("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("CancelTask("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("CleanupExpiredTasks("), std::string::npos);
    EXPECT_EQ(runtime_execution_header.find("StopExecution("), std::string::npos);
    EXPECT_NE(runtime_execution_header.find("DispensingExecutionResult"), std::string::npos);
}
