#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {
using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;
using Siligen::Shared::Types::ErrorCode;
}  // namespace

TEST(UploadFileUseCaseTest, ArchivesSourceDrawingWithSourceHashAndValidationReport) {
    ScopedTempDir workspace("upload_source_success");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest());
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& source = result.Value();
    const auto dxf_path = std::filesystem::path(source.filepath);

    EXPECT_TRUE(std::filesystem::exists(dxf_path));
    EXPECT_EQ(dxf_path.filename().string(), source.generated_filename);
    EXPECT_FALSE(source.source_hash.empty());
    EXPECT_EQ(source.source_drawing_ref, "sha256:" + source.source_hash);
    EXPECT_EQ(source.validation_report.schema_version, "DXFValidationReport.v1");
    EXPECT_EQ(source.validation_report.stage_id, "S1");
    EXPECT_EQ(source.validation_report.owner_module, "M1");
    EXPECT_EQ(source.validation_report.source_ref, source.source_drawing_ref);
    EXPECT_EQ(source.validation_report.source_hash, source.source_hash);
    EXPECT_EQ(source.validation_report.gate_result, "PASS");
    EXPECT_EQ(source.validation_report.result_classification, "source_drawing_accepted");
    EXPECT_FALSE(source.validation_report.preview_ready);
    EXPECT_FALSE(source.validation_report.production_ready);
}

TEST(UploadFileUseCaseTest, RejectsUnreadableDxfPayloadAndCleansStoredSource) {
    ScopedTempDir workspace("upload_invalid_source_cleanup");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto request = MakeUploadRequest();
    const std::string invalid_payload = "this payload is definitely not a readable dxf document";
    request.file_content.assign(invalid_payload.begin(), invalid_payload.end());
    request.file_size = request.file_content.size();

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::FILE_FORMAT_INVALID);

    size_t dxf_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(storage->BaseDir())) {
        if (entry.path().extension() == ".dxf") {
            ++dxf_count;
        }
    }
    EXPECT_EQ(dxf_count, 0u);
}
