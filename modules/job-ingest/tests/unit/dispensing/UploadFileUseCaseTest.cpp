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
}  // namespace

TEST(UploadFileUseCaseTest, GeneratesPbAfterUpload) {
    ScopedTempDir workspace("upload_success");
    const auto generator_path = workspace.Path() / "gen_pb.py";
    WriteTextFile(generator_path,
                  "import pathlib\n"
                  "import sys\n"
                  "pathlib.Path(sys.argv[2]).write_bytes(b'pb')\n");

    const ScopedEnvVar command_override(
        "SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto dxf_path = std::filesystem::path(result.Value().filepath);
    const auto pb_path = PbPathFor(dxf_path);

    EXPECT_TRUE(std::filesystem::exists(dxf_path));
    EXPECT_TRUE(std::filesystem::exists(pb_path));
    EXPECT_EQ(ReadTextFile(pb_path), "pb");
    EXPECT_EQ(dxf_path.filename().string(), result.Value().generated_filename);
}

TEST(UploadFileUseCaseTest, CleansUpArtifactsWhenPbGenerationFails) {
    ScopedTempDir workspace("upload_failure_cleanup");
    const auto generator_path = workspace.Path() / "fail_pb.py";
    WriteTextFile(generator_path, "raise SystemExit(7)\n");

    const ScopedEnvVar command_override(
        "SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest());
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::COMMAND_FAILED);

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

TEST(UploadFileUseCaseTest, RejectsTooSmallPayloadBeforePersistingArtifacts) {
    ScopedTempDir workspace("upload_invalid_small");
    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto request = MakeUploadRequest();
    const std::string invalid_payload = "short";
    request.file_content.assign(invalid_payload.begin(), invalid_payload.end());
    request.file_size = request.file_content.size();

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);

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

TEST(UploadFileUseCaseTest, RejectsNonDxfPayloadWithoutLeavingArtifacts) {
    ScopedTempDir workspace("upload_invalid_content");
    auto storage = std::make_shared<TestFileStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto request = MakeUploadRequest();
    const std::string invalid_payload = "this payload is definitely not a valid dxf document or binary signature";
    request.file_content.assign(invalid_payload.begin(), invalid_payload.end());
    request.file_size = request.file_content.size();

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);

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
