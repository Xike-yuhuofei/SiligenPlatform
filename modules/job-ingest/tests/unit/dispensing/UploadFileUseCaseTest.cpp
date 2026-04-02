#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "job_ingest/contracts/storage/IFileStoragePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
using Siligen::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::Application::UseCases::Dispensing::UploadRequest;
using Siligen::JobIngest::Contracts::Storage::FileData;
using Siligen::JobIngest::Contracts::Storage::IFileStoragePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class TestFileStoragePort final : public IFileStoragePort {
   public:
    explicit TestFileStoragePort(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
        std::filesystem::create_directories(base_dir_);
    }

    Result<std::string> StoreFile(const FileData& file_data, const std::string& filename) override {
        if (file_data.content.empty()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "file content empty", "TestFileStoragePort"));
        }
        std::filesystem::create_directories(base_dir_);
        std::filesystem::path full_path = base_dir_ / filename;
        std::ofstream out(full_path, std::ios::binary);
        if (!out.good()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "failed to open file", "TestFileStoragePort"));
        }
        out.write(reinterpret_cast<const char*>(file_data.content.data()),
                  static_cast<std::streamsize>(file_data.content.size()));
        out.close();
        return Result<std::string>::Success(full_path.string());
    }

    Result<void> ValidateFile(const FileData&, size_t, const std::vector<std::string>&) override {
        return Result<void>::Success();
    }

    Result<void> DeleteFile(const std::string& filepath) override {
        std::error_code ec;
        std::filesystem::remove(filepath, ec);
        if (ec) {
            return Result<void>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "delete failed", "TestFileStoragePort"));
        }
        return Result<void>::Success();
    }

    Result<bool> FileExists(const std::string& filepath) override {
        std::error_code ec;
        bool exists = std::filesystem::exists(filepath, ec);
        if (ec) {
            return Result<bool>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "exists check failed", "TestFileStoragePort"));
        }
        return Result<bool>::Success(exists);
    }

    Result<size_t> GetFileSize(const std::string& filepath) override {
        std::error_code ec;
        auto size = std::filesystem::file_size(filepath, ec);
        if (ec) {
            return Result<size_t>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "size check failed", "TestFileStoragePort"));
        }
        return Result<size_t>::Success(static_cast<size_t>(size));
    }

   private:
    std::filesystem::path base_dir_;
};

void SetEnvVar(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

void UnsetEnvVar(const std::string& name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

std::string QuoteArg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::filesystem::path MakeTempDir(const std::string& name) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_" + name + "_" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path;
}

std::string MinimalDxf() {
    return "0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n0\n20\n0\n11\n10\n21\n0\n0\nENDSEC\n0\nEOF\n";
}

void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}
}  // namespace

TEST(UploadFileUseCaseTest, GeneratesPbAfterUpload) {
    const auto base_dir = MakeTempDir("upload_success");
    const auto generator_path = base_dir / "gen_pb.py";
    WriteTextFile(generator_path,
                  "import pathlib\n"
                  "import sys\n"
                  "pathlib.Path(sys.argv[2]).write_bytes(b'pb')\n");

    std::string command = "python " + QuoteArg(generator_path.string()) + " {input} {output}";
    SetEnvVar("SILIGEN_DXF_PB_COMMAND", command);

    UploadRequest request;
    request.original_filename = "unit_test.dxf";
    request.content_type = "application/dxf";
    const std::string minimal_dxf = MinimalDxf();
    request.file_content.assign(minimal_dxf.begin(), minimal_dxf.end());
    request.file_size = request.file_content.size();

    auto storage = std::make_shared<TestFileStoragePort>(base_dir);
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    std::filesystem::path dxf_path(result.Value().filepath);
    std::filesystem::path pb_path = dxf_path;
    pb_path.replace_extension(".pb");

    EXPECT_TRUE(std::filesystem::exists(dxf_path));
    EXPECT_TRUE(std::filesystem::exists(pb_path));

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove(dxf_path, ec);
    std::filesystem::remove(pb_path, ec);
    std::filesystem::remove(generator_path, ec);
    std::filesystem::remove_all(base_dir, ec);
}

TEST(UploadFileUseCaseTest, CleansUpArtifactsWhenPbGenerationFails) {
    const auto base_dir = MakeTempDir("upload_failure_cleanup");
    const auto generator_path = base_dir / "fail_pb.py";
    WriteTextFile(generator_path, "raise SystemExit(7)\n");

    SetEnvVar("SILIGEN_DXF_PB_COMMAND", "python " + QuoteArg(generator_path.string()) + " {input} {output}");

    UploadRequest request;
    request.original_filename = "unit_test.dxf";
    request.content_type = "application/dxf";
    const std::string minimal_dxf = MinimalDxf();
    request.file_content.assign(minimal_dxf.begin(), minimal_dxf.end());
    request.file_size = request.file_content.size();

    auto storage = std::make_shared<TestFileStoragePort>(base_dir);
    UploadFileUseCase usecase(storage);

    auto result = usecase.Execute(request);
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::COMMAND_FAILED);

    size_t dxf_count = 0;
    size_t pb_count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
        if (entry.path().extension() == ".dxf") {
            ++dxf_count;
        }
        if (entry.path().extension() == ".pb") {
            ++pb_count;
        }
    }
    EXPECT_EQ(dxf_count, 0u);
    EXPECT_EQ(pb_count, 0u);

    UnsetEnvVar("SILIGEN_DXF_PB_COMMAND");

    std::error_code ec;
    std::filesystem::remove_all(base_dir, ec);
}
