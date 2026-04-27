#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::ComputeExpectedSourceHash;
using Siligen::JobIngest::Tests::Support::HasExtensionFile;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::ScopedEnvVar;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;

}  // namespace

TEST(UploadFileUseCaseIntegrationTest, IgnoresLegacyPbCommandOverrideAndStillArchivesSourceDrawing) {
    ScopedTempDir workspace("upload_integration_env_override");
    const ScopedEnvVar command_override("SILIGEN_DXF_PB_COMMAND", "python fake_generator.py");

    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    const auto request = MakeUploadRequest("fixture upload (v1).dxf");
    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& drawing = result.Value();
    const auto stored_path = std::filesystem::path(drawing.filepath);

    EXPECT_TRUE(std::filesystem::exists(stored_path));
    EXPECT_EQ(stored_path.filename().string(), drawing.generated_filename);
    EXPECT_NE(drawing.generated_filename.find("fixtureuploadv1.dxf"), std::string::npos);
    EXPECT_EQ(drawing.source_drawing_ref, "source-drawing:" + drawing.generated_filename);
    EXPECT_EQ(drawing.source_hash, ComputeExpectedSourceHash(request));
    EXPECT_FALSE(HasExtensionFile(storage->BaseDir(), ".pb"));
    EXPECT_EQ(drawing.validation_report.stage_id, "S1");
    EXPECT_EQ(drawing.validation_report.owner_module, "M1");
}

TEST(UploadFileUseCaseIntegrationTest, PreservesCanonicalValidationReportShapeAcrossRealFilesystemRoundTrip) {
    ScopedTempDir workspace("upload_integration_roundtrip");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("canonical-contract.dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& drawing = result.Value();
    EXPECT_EQ(drawing.validation_report.schema_version, "DXFValidationReport.v1");
    EXPECT_EQ(drawing.validation_report.stage_id, "S1");
    EXPECT_EQ(drawing.validation_report.owner_module, "M1");
    EXPECT_EQ(drawing.validation_report.source_ref, drawing.source_drawing_ref);
    EXPECT_EQ(drawing.validation_report.source_hash, drawing.source_hash);
    EXPECT_EQ(drawing.validation_report.gate_result, "PASS");
    EXPECT_EQ(drawing.validation_report.result_classification, "source_drawing_accepted");
    EXPECT_FALSE(drawing.validation_report.formal_compare_gate.has_value);
}
