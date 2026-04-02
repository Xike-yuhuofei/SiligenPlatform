#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using Siligen::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::PbPathFor;
using Siligen::JobIngest::Tests::Support::QuoteArg;
using Siligen::JobIngest::Tests::Support::ReadTextFile;
using Siligen::JobIngest::Tests::Support::ScopedEnvVar;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestFileStoragePort;
using Siligen::JobIngest::Tests::Support::WriteTextFile;
using Siligen::Shared::Types::ErrorCode;

TEST(UploadFileUseCaseIntegrationTest, ProducesSanitizedArtifactsThroughPbCommandOverride) {
    ScopedTempDir workspace("upload_integration_success");
    const auto generator_path = workspace.Path() / "emit_pb.py";
    WriteTextFile(generator_path,
                  "import pathlib\n"
                  "import sys\n"
                  "input_path = pathlib.Path(sys.argv[1])\n"
                  "output_path = pathlib.Path(sys.argv[2])\n"
                  "output_path.write_bytes(('pb:' + input_path.stem).encode())\n");

    const ScopedEnvVar command_override(
        "SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("fixture upload (v1).dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& response = result.Value();
    const auto dxf_path = std::filesystem::path(response.filepath);
    const auto pb_path = PbPathFor(dxf_path);

    ASSERT_TRUE(std::filesystem::exists(dxf_path));
    ASSERT_TRUE(std::filesystem::exists(pb_path));
    EXPECT_EQ(dxf_path.filename().string(), response.generated_filename);
    EXPECT_EQ(response.original_name, "fixture upload (v1).dxf");
    EXPECT_NE(response.generated_filename.find("fixtureuploadv1.dxf"), std::string::npos);
    EXPECT_EQ(pb_path.stem().string(), dxf_path.stem().string());
    EXPECT_EQ(ReadTextFile(pb_path), "pb:" + dxf_path.stem().string());
}

TEST(UploadFileUseCaseIntegrationTest, PropagatesPbCommandConfigurationErrorsAndRemovesArtifacts) {
    ScopedTempDir workspace("upload_integration_config_error");
    const ScopedEnvVar command_override("SILIGEN_DXF_PB_COMMAND", "python fake_generator.py");

    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("config failure.dxf"));
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::CONFIGURATION_ERROR);

    size_t dxf_count = 0;
    size_t pb_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(storage->BaseDir())) {
        if (entry.path().extension() == ".dxf") {
            ++dxf_count;
        }
        if (entry.path().extension() == ".pb") {
            ++pb_count;
        }
    }
    EXPECT_EQ(dxf_count, 0u);
    EXPECT_EQ(pb_count, 0u);
}

}  // namespace
