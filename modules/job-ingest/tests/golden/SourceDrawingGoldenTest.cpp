#include "support/UploadFileUseCaseTestSupport.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

namespace {

using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Contracts::SourceDrawing;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "source_drawing.summary.txt";
}

std::string ReadGoldenFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.is_open()) << "missing golden file: " << path.string();
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string NormalizeDynamicFilename(const std::string& raw_value) {
    const std::string filename = std::filesystem::path(raw_value).filename().string();
    const size_t first_separator = filename.find('_');
    const size_t second_separator =
        first_separator == std::string::npos ? std::string::npos : filename.find('_', first_separator + 1);

    EXPECT_NE(first_separator, std::string::npos);
    EXPECT_NE(second_separator, std::string::npos);
    if (first_separator == std::string::npos || second_separator == std::string::npos) {
        return filename;
    }

    return std::string("<uuid>_<timestamp>") + filename.substr(second_separator);
}

std::string SerializeSummary(const SourceDrawing& source) {
    std::ostringstream out;
    out << std::boolalpha;
    out << "source_drawing_ref_prefix=" << source.source_drawing_ref.substr(0, 7) << "\n";
    out << "source_hash_size=" << source.source_hash.size() << "\n";
    out << "original_name=" << source.original_name << "\n";
    out << "size=" << source.size << "\n";
    out << "generated_filename=" << NormalizeDynamicFilename(source.generated_filename) << "\n";
    out << "stored_filename=" << NormalizeDynamicFilename(source.filepath) << "\n";
    out << "validation_schema_version=" << source.validation_report.schema_version << "\n";
    out << "validation_stage_id=" << source.validation_report.stage_id << "\n";
    out << "validation_owner_module=" << source.validation_report.owner_module << "\n";
    out << "validation_gate_result=" << source.validation_report.gate_result << "\n";
    out << "validation_result_classification=" << source.validation_report.result_classification << "\n";
    out << "validation_preview_ready=" << source.validation_report.preview_ready << "\n";
    out << "validation_production_ready=" << source.validation_report.production_ready << "\n";
    return out.str();
}

TEST(SourceDrawingGoldenTest, SanitizedSourceDrawingSummaryMatchesGoldenSnapshot) {
    ScopedTempDir workspace("source_drawing_golden");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("golden upload.dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto actual = SerializeSummary(result.Value());
    const auto expected = ReadGoldenFile(GoldenPath());
    EXPECT_EQ(actual, expected);
}

}  // namespace
