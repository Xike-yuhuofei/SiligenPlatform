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

TEST(UploadFileUseCaseIntegrationTest, StoresCanonicalSourceDrawingWithoutPreparingPlanningInput) {
    ScopedTempDir workspace("upload_integration_source_success");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("fixture upload (v1).dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& source = result.Value();
    const auto dxf_path = std::filesystem::path(source.filepath);
    const auto pb_path = dxf_path.parent_path() / (dxf_path.stem().string() + ".pb");

    ASSERT_TRUE(std::filesystem::exists(dxf_path));
    EXPECT_FALSE(std::filesystem::exists(pb_path));
    EXPECT_EQ(dxf_path.filename().string(), source.generated_filename);
    EXPECT_EQ(source.original_name, "fixture upload (v1).dxf");
    EXPECT_NE(source.generated_filename.find("fixtureuploadv1.dxf"), std::string::npos);
    EXPECT_EQ(source.source_drawing_ref, "sha256:" + source.source_hash);
    EXPECT_EQ(source.validation_report.source_ref, source.source_drawing_ref);
    EXPECT_EQ(source.validation_report.source_hash, source.source_hash);
    EXPECT_NE(source.validation_report.summary.find("Source drawing accepted"), std::string::npos);
}

TEST(UploadFileUseCaseIntegrationTest, RejectsInvalidSourceDrawingAndRemovesStoredFile) {
    ScopedTempDir workspace("upload_integration_invalid_source");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto request = MakeUploadRequest("invalid source.dxf");
    const std::string invalid_payload = "not a readable dxf payload";
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

}  // namespace
