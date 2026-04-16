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
using Siligen::JobIngest::Tests::Support::FakeUploadPreparationPort;
using Siligen::JobIngest::Tests::Support::MakeUploadRequest;
using Siligen::JobIngest::Tests::Support::PbPathFor;
using Siligen::JobIngest::Tests::Support::ReadTextFile;
using Siligen::JobIngest::Tests::Support::ScopedTempDir;
using Siligen::JobIngest::Tests::Support::TestUploadStoragePort;
using Siligen::JobIngest::Tests::Support::UploadResponse;

std::filesystem::path GoldenPath() {
    return std::filesystem::path(__FILE__).parent_path() / "upload_response.summary.txt";
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

std::string SerializeSummary(const UploadResponse& response, const std::filesystem::path& pb_path) {
    std::ostringstream out;
    out << std::boolalpha;
    out << "success=" << response.success << "\n";
    out << "original_name=" << response.original_name << "\n";
    out << "size=" << response.size << "\n";
    out << "generated_filename=" << NormalizeDynamicFilename(response.generated_filename) << "\n";
    out << "stored_filename=" << NormalizeDynamicFilename(response.filepath) << "\n";
    out << "prepared_filename=" << NormalizeDynamicFilename(response.prepared_filepath) << "\n";
    out << "import_result_classification=" << response.import_diagnostics.result_classification << "\n";
    out << "import_preview_ready=" << response.import_diagnostics.preview_ready << "\n";
    out << "import_production_ready=" << response.import_diagnostics.production_ready << "\n";
    out << "import_primary_code=" << response.import_diagnostics.primary_code << "\n";
    out << "import_summary=" << response.import_diagnostics.summary << "\n";
    out << "pb_filename=" << NormalizeDynamicFilename(pb_path.string()) << "\n";
    out << "pb_size=" << std::filesystem::file_size(pb_path) << "\n";
    out << "pb_contents=" << ReadTextFile(pb_path) << "\n";
    return out.str();
}

TEST(UploadResponseGoldenTest, SanitizedUploadSummaryMatchesGoldenSnapshot) {
    ScopedTempDir workspace("upload_golden");
    auto storage = std::make_shared<TestUploadStoragePort>(workspace.Path() / "uploads");
    auto preparation = std::make_shared<FakeUploadPreparationPort>();
    UploadFileUseCase usecase(storage, preparation);

    auto result = usecase.Execute(MakeUploadRequest("golden upload.dxf"));
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto pb_path = PbPathFor(result.Value().filepath);
    ASSERT_TRUE(std::filesystem::exists(pb_path));

    const auto actual = SerializeSummary(result.Value(), pb_path);
    const auto expected = ReadGoldenFile(GoldenPath());
    EXPECT_EQ(actual, expected);
}

}  // namespace
