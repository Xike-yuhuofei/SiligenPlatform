#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {
using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::FakeUploadPreparationPort;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::PbPathFor;
using Siligen::JobIngest::Tests::Support::ReadTextFile;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;
using Siligen::Shared::Types::ErrorCode;
}  // namespace

TEST(UploadFileUseCaseTest, GeneratesPbAfterUpload) {
    ScopedTempDir workspace("upload_success");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<FakeUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

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
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<FakeUploadPreparationPort>(
        [](const std::string& source_path) {
            const auto pb_path = PbPathFor(source_path);
            std::ofstream out(pb_path, std::ios::binary);
            out << "pb";
            out.close();
            return Siligen::Shared::Types::Result<std::string>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::COMMAND_FAILED,
                    "pb generation failed",
                    "FakeUploadPreparationPort"));
        });
    UploadFileUseCase usecase(storage, preparation);

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
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<FakeUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

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
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<FakeUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

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
