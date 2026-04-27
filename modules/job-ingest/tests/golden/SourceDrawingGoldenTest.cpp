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
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::ReadTextFile;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::SourceDrawing;
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

std::string NormalizeSourceDrawingRef(const std::string& raw_value) {
    constexpr const char* prefix = "source-drawing:";
    const std::string prefix_value(prefix);
    if (raw_value.rfind(prefix_value, 0) != 0) {
        return raw_value;
    }
    return prefix_value + NormalizeDynamicFilename(raw_value.substr(prefix_value.size()));
}

std::string SerializeSummary(const SourceDrawing& drawing) {
    std::ostringstream out;
    out << std::boolalpha;
    out << "source_drawing_ref=" << NormalizeSourceDrawingRef(drawing.source_drawing_ref) << "\n";
    out << "original_name=" << drawing.original_name << "\n";
    out << "size=" << drawing.size << "\n";
    out << "generated_filename=" << NormalizeDynamicFilename(drawing.generated_filename) << "\n";
    out << "stored_filename=" << NormalizeDynamicFilename(drawing.filepath) << "\n";
    out << "source_hash=" << drawing.source_hash << "\n";
    out << "validation_schema_version=" << drawing.validation_report.schema_version << "\n";
    out << "validation_stage_id=" << drawing.validation_report.stage_id << "\n";
    out << "validation_owner_module=" << drawing.validation_report.owner_module << "\n";
    out << "validation_gate_result=" << drawing.validation_report.gate_result << "\n";
    out << "validation_result_classification=" << drawing.validation_report.result_classification << "\n";
    out << "validation_preview_ready=" << drawing.validation_report.preview_ready << "\n";
    out << "validation_production_ready=" << drawing.validation_report.production_ready << "\n";
    out << "validation_summary=" << drawing.validation_report.summary << "\n";
    out << "stored_contents=" << ReadTextFile(drawing.filepath) << "\n";
    return out.str();
}

TEST(SourceDrawingGoldenTest, SanitizedUploadSummaryMatchesGoldenSnapshot) {
    ScopedTempDir workspace("upload_golden");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(MakeUploadRequest("golden upload.dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto actual = SerializeSummary(result.Value());
    const auto expected = ReadGoldenFile(GoldenPath());
    EXPECT_EQ(actual, expected);
}

}  // namespace
