#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::ComputeExpectedSourceHash;
using Siligen::JobIngest::Tests::Support::HasAnyFiles;
using Siligen::JobIngest::Tests::Support::HasExtensionFile;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::ReadTextFile;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::Shared::Types::ErrorCode;

}  // namespace

TEST(UploadFileUseCaseTest, ArchivesSourceDrawingAndEmitsS1ValidationReport) {
    ScopedTempDir workspace("upload_success");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    const auto request = MakeUploadRequest();
    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& drawing = result.Value();
    const auto stored_path = std::filesystem::path(drawing.filepath);

    EXPECT_TRUE(std::filesystem::exists(stored_path));
    EXPECT_EQ(ReadTextFile(stored_path), std::string(request.file_content.begin(), request.file_content.end()));
    EXPECT_EQ(stored_path.filename().string(), drawing.generated_filename);
    EXPECT_EQ(drawing.source_drawing_ref, "source-drawing:" + drawing.generated_filename);
    EXPECT_EQ(drawing.original_name, request.original_filename);
    EXPECT_EQ(drawing.size, request.file_size);
    EXPECT_EQ(drawing.source_hash, ComputeExpectedSourceHash(request));
    EXPECT_EQ(drawing.validation_report.stage_id, "S1");
    EXPECT_EQ(drawing.validation_report.owner_module, "M1");
    EXPECT_EQ(drawing.validation_report.source_ref, drawing.source_drawing_ref);
    EXPECT_EQ(drawing.validation_report.source_hash, drawing.source_hash);
    EXPECT_EQ(drawing.validation_report.gate_result, "PASS");
    EXPECT_EQ(drawing.validation_report.result_classification, "source_drawing_accepted");
    EXPECT_FALSE(drawing.validation_report.preview_ready);
    EXPECT_FALSE(drawing.validation_report.production_ready);
}

TEST(UploadFileUseCaseTest, RejectsOversizedSourceDrawingWithoutPersistingFiles) {
    ScopedTempDir workspace("upload_too_large");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage, 1);

    auto request = MakeUploadRequest();
    request.file_content.resize(2 * 1024 * 1024, static_cast<std::uint8_t>('A'));
    request.file_size = request.file_content.size();

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_FALSE(HasAnyFiles(storage->BaseDir()));
}

TEST(UploadFileUseCaseTest, RejectsUnsupportedExtensionWithoutPersistingFiles) {
    ScopedTempDir workspace("upload_invalid_extension");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto request = MakeUploadRequest("fixture.txt");
    auto result = usecase.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);
    EXPECT_FALSE(HasAnyFiles(storage->BaseDir()));
}

TEST(UploadFileUseCaseTest, DoesNotSniffPayloadAndDoesNotCreatePreparedArtifacts) {
    ScopedTempDir workspace("upload_raw_archive_only");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    UploadRequest request;
    const std::string invalid_payload = "this is not a valid dxf body";
    request.original_filename = "opaque.dxf";
    request.content_type = "application/dxf";
    request.file_content.assign(invalid_payload.begin(), invalid_payload.end());
    request.file_size = request.file_content.size();

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_FALSE(HasExtensionFile(storage->BaseDir(), ".pb"));
    EXPECT_EQ(result.Value().source_hash, ComputeExpectedSourceHash(request));
    EXPECT_EQ(result.Value().validation_report.result_classification, "source_drawing_accepted");
}
