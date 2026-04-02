#pragma once

#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "job_ingest/contracts/storage/IFileStoragePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::JobIngest::Tests::Support {

using Siligen::Application::UseCases::Dispensing::UploadRequest;
using Siligen::Application::UseCases::Dispensing::UploadResponse;
using Siligen::JobIngest::Contracts::Storage::FileData;
using Siligen::JobIngest::Contracts::Storage::IFileStoragePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

inline void SetEnvVar(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

inline void UnsetEnvVar(const std::string& name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

class ScopedEnvVar {
   public:
    ScopedEnvVar(std::string name, std::string value) : name_(std::move(name)) {
        if (const char* current = std::getenv(name_.c_str())) {
            original_value_ = std::string(current);
        }
        SetEnvVar(name_, value);
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

    ~ScopedEnvVar() {
        if (original_value_.has_value()) {
            SetEnvVar(name_, *original_value_);
            return;
        }
        UnsetEnvVar(name_);
    }

   private:
    std::string name_;
    std::optional<std::string> original_value_;
};

class ScopedTempDir {
   public:
    explicit ScopedTempDir(std::string name) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / ("siligen_" + std::move(name) + "_" + std::to_string(now));
        std::filesystem::create_directories(path_);
    }

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    ~ScopedTempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    [[nodiscard]] const std::filesystem::path& Path() const { return path_; }

   private:
    std::filesystem::path path_;
};

inline std::string QuoteArg(const std::string& value) {
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

inline std::string MinimalDxf() {
    return "0\nSECTION\n2\nENTITIES\n0\nLINE\n10\n0\n20\n0\n11\n10\n21\n0\n0\nENDSEC\n0\nEOF\n";
}

inline void WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::error_code ec;
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }
    std::ofstream out(path, std::ios::binary);
    out << content;
}

inline std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline std::filesystem::path PbPathFor(std::filesystem::path dxf_path) {
    dxf_path.replace_extension(".pb");
    return dxf_path;
}

inline std::string NormalizeExtension(std::string extension) {
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(extension.begin());
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

class TestFileStoragePort final : public IFileStoragePort {
   public:
    explicit TestFileStoragePort(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
        std::filesystem::create_directories(base_dir_);
    }

    [[nodiscard]] const std::filesystem::path& BaseDir() const { return base_dir_; }

    Result<std::string> StoreFile(const FileData& file_data, const std::string& filename) override {
        if (file_data.content.empty()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "file content empty", "TestFileStoragePort"));
        }

        std::filesystem::path full_path = base_dir_ / filename;
        std::error_code ec;
        std::filesystem::create_directories(full_path.parent_path(), ec);

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

    Result<void> ValidateFile(const FileData& file_data,
                              size_t max_size_mb,
                              const std::vector<std::string>& allowed_extensions) override {
        if (file_data.content.empty() || file_data.size == 0 || file_data.size != file_data.content.size()) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "invalid file metadata", "TestFileStoragePort"));
        }

        if (max_size_mb > 0) {
            const size_t max_size_bytes = max_size_mb * 1024u * 1024u;
            if (file_data.size > max_size_bytes) {
                return Result<void>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "file too large", "TestFileStoragePort"));
            }
        }

        if (!allowed_extensions.empty()) {
            const std::string extension = NormalizeExtension(std::filesystem::path(file_data.original_name).extension().string());
            const bool allowed = std::any_of(
                allowed_extensions.begin(), allowed_extensions.end(), [&extension](const std::string& candidate) {
                    return NormalizeExtension(candidate) == extension;
                });
            if (!allowed) {
                return Result<void>::Failure(
                    Error(ErrorCode::FILE_FORMAT_INVALID, "extension not allowed", "TestFileStoragePort"));
            }
        }

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
        const bool exists = std::filesystem::exists(filepath, ec);
        if (ec) {
            return Result<bool>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "exists check failed", "TestFileStoragePort"));
        }
        return Result<bool>::Success(exists);
    }

    Result<size_t> GetFileSize(const std::string& filepath) override {
        std::error_code ec;
        const auto size = std::filesystem::file_size(filepath, ec);
        if (ec) {
            return Result<size_t>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "size check failed", "TestFileStoragePort"));
        }
        return Result<size_t>::Success(static_cast<size_t>(size));
    }

   private:
    std::filesystem::path base_dir_;
};

inline UploadRequest MakeUploadRequest(const std::string& original_filename = "unit_test.dxf") {
    UploadRequest request;
    request.original_filename = original_filename;
    request.content_type = "application/dxf";
    const std::string minimal_dxf = MinimalDxf();
    request.file_content.assign(minimal_dxf.begin(), minimal_dxf.end());
    request.file_size = request.file_content.size();
    return request;
}

}  // namespace Siligen::JobIngest::Tests::Support
