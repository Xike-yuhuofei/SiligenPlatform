#include "support/UploadFileUseCaseTestSupport.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadPreparationPort;
using Siligen::JobIngest::Application::Ports::Dispensing::PreparedInputArtifact;
using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::PbPathFor;
using Siligen::JobIngest::Tests::Support::QuoteArg;
using Siligen::JobIngest::Tests::Support::ScopedEnvVar;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;
using Siligen::JobIngest::Tests::Support::WriteTextFile;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

std::filesystem::path RepoRootPath() {
    auto root = std::filesystem::path(__FILE__).parent_path();
    for (int depth = 0; depth < 4; ++depth) {
        root = root.parent_path();
    }
    return root;
}

class RealUploadPreparationPort final : public IUploadPreparationPort {
   public:
    Result<PreparedInputArtifact> EnsurePreparedInput(const std::string& source_path) const override {
        auto result = service_.PrepareInputArtifact(source_path);
        if (result.IsError()) {
            return Result<PreparedInputArtifact>::Failure(result.GetError());
        }
        PreparedInputArtifact artifact;
        artifact.prepared_path = result.Value().prepared_path;
        artifact.import_diagnostics.result_classification = result.Value().import_diagnostics.result_classification;
        artifact.import_diagnostics.preview_ready = result.Value().import_diagnostics.preview_ready;
        artifact.import_diagnostics.production_ready = result.Value().import_diagnostics.production_ready;
        artifact.import_diagnostics.summary = result.Value().import_diagnostics.summary;
        artifact.import_diagnostics.primary_code = result.Value().import_diagnostics.primary_code;
        artifact.import_diagnostics.warning_codes = result.Value().import_diagnostics.warning_codes;
        artifact.import_diagnostics.error_codes = result.Value().import_diagnostics.error_codes;
        artifact.import_diagnostics.resolved_units = result.Value().import_diagnostics.resolved_units;
        artifact.import_diagnostics.resolved_unit_scale = result.Value().import_diagnostics.resolved_unit_scale;
        return Result<PreparedInputArtifact>::Success(std::move(artifact));
    }

    Result<void> CleanupPreparedInput(const std::string& source_path) const override {
        return service_.CleanupPreparedInput(source_path);
    }

   private:
    DxfPbPreparationService service_;
};

TEST(UploadFileUseCaseIntegrationTest, ProducesSanitizedArtifactsThroughPbCommandOverride) {
    ScopedTempDir workspace("upload_integration_success");
    const auto generator_path = workspace.Path() / "emit_pb.py";
    const auto proto_root =
        (RepoRootPath() / "modules" / "dxf-geometry" / "application").generic_string();
    WriteTextFile(generator_path,
                  "import pathlib\n"
                  "import sys\n"
                  "sys.path.insert(0, r'" + proto_root + "')\n"
                  "from engineering_data.proto import dxf_primitives_pb2 as pb\n"
                  "input_path = pathlib.Path(sys.argv[1])\n"
                  "output_path = pathlib.Path(sys.argv[2])\n"
                  "bundle = pb.PathBundle()\n"
                  "bundle.header.schema_version = 1\n"
                  "bundle.header.source_path = str(input_path)\n"
                  "bundle.header.units = 'mm'\n"
                  "bundle.header.unit_scale = 1.0\n"
                  "line = bundle.primitives.add()\n"
                  "line.type = pb.PRIMITIVE_LINE\n"
                  "line.line.start.x = 0.0\n"
                  "line.line.start.y = 0.0\n"
                  "line.line.end.x = 10.0\n"
                  "line.line.end.y = 0.0\n"
                  "bundle.import_diagnostics.classification = pb.IMPORT_RESULT_SUCCESS\n"
                  "bundle.import_diagnostics.preview_ready = True\n"
                  "bundle.import_diagnostics.production_ready = True\n"
                  "bundle.import_diagnostics.summary = 'DXF import succeeded and is ready for production.'\n"
                  "bundle.import_diagnostics.resolved_units = 'mm'\n"
                  "bundle.import_diagnostics.resolved_unit_scale = 1.0\n"
                  "output_path.write_bytes(bundle.SerializeToString())\n");

    const ScopedEnvVar command_override(
        "SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<RealUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

    auto result = usecase.Execute(MakeUploadRequest("fixture upload (v1).dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& response = result.Value();
    const auto dxf_path = std::filesystem::path(response.filepath);
    const auto pb_path = PbPathFor(dxf_path);

    ASSERT_TRUE(std::filesystem::exists(dxf_path));
    ASSERT_TRUE(std::filesystem::exists(pb_path));
    EXPECT_EQ(dxf_path.filename().string(), response.generated_filename);
    EXPECT_EQ(response.prepared_filepath, pb_path.string());
    EXPECT_EQ(response.original_name, "fixture upload (v1).dxf");
    EXPECT_NE(response.generated_filename.find("fixtureuploadv1.dxf"), std::string::npos);
    EXPECT_EQ(pb_path.stem().string(), dxf_path.stem().string());
    EXPECT_EQ(response.import_diagnostics.result_classification, "success");
    EXPECT_TRUE(response.import_diagnostics.preview_ready);
    EXPECT_TRUE(response.import_diagnostics.production_ready);
    EXPECT_EQ(
        response.import_diagnostics.summary,
        "DXF import succeeded and is ready for production.");
    EXPECT_EQ(response.import_diagnostics.resolved_units, "mm");
    EXPECT_DOUBLE_EQ(response.import_diagnostics.resolved_unit_scale, 1.0);
    EXPECT_GT(std::filesystem::file_size(pb_path), 0U);
}

TEST(UploadFileUseCaseIntegrationTest, PropagatesPbCommandConfigurationErrorsAndRemovesArtifacts) {
    ScopedTempDir workspace("upload_integration_config_error");
    const ScopedEnvVar command_override("SILIGEN_DXF_PB_COMMAND", "python fake_generator.py");

    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<RealUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

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
