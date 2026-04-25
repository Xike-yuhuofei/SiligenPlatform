#pragma once

#include "job_ingest/application/ports/dispensing/UploadPorts.h"
#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Siligen::JobIngest::Tests::Support {

using Siligen::JobIngest::Application::Ports::Dispensing::IUploadPreparationPort;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadStoragePort;
using Siligen::JobIngest::Application::Ports::Dispensing::PreparedInputArtifact;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::JobIngest::Contracts::UploadResponse;
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

class TestUploadStoragePort final : public IUploadStoragePort {
   public:
    explicit TestUploadStoragePort(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
        std::filesystem::create_directories(base_dir_);
    }

    [[nodiscard]] const std::filesystem::path& BaseDir() const { return base_dir_; }

    Result<void> Validate(const UploadRequest& request,
                          size_t max_size_mb,
                          const std::vector<std::string>& allowed_extensions) const override {
        if (request.file_content.empty() || request.file_size == 0 || request.file_size != request.file_content.size()) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "invalid file metadata", "TestUploadStoragePort"));
        }

        if (max_size_mb > 0) {
            const size_t max_size_bytes = max_size_mb * 1024u * 1024u;
            if (request.file_size > max_size_bytes) {
                return Result<void>::Failure(
                    Error(ErrorCode::INVALID_PARAMETER, "file too large", "TestUploadStoragePort"));
            }
        }

        if (!allowed_extensions.empty()) {
            const std::string extension = NormalizeExtension(std::filesystem::path(request.original_filename).extension().string());
            const bool allowed = std::any_of(
                allowed_extensions.begin(), allowed_extensions.end(), [&extension](const std::string& candidate) {
                    return NormalizeExtension(candidate) == extension;
                });
            if (!allowed) {
                return Result<void>::Failure(
                    Error(ErrorCode::FILE_FORMAT_INVALID, "extension not allowed", "TestUploadStoragePort"));
            }
        }

        return Result<void>::Success();
    }

    Result<std::string> Store(const UploadRequest& request, const std::string& filename) override {
        if (request.file_content.empty()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "file content empty", "TestUploadStoragePort"));
        }

        std::filesystem::path full_path = base_dir_ / filename;
        std::error_code ec;
        std::filesystem::create_directories(full_path.parent_path(), ec);

        std::ofstream out(full_path, std::ios::binary);
        if (!out.good()) {
            return Result<std::string>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "failed to open file", "TestUploadStoragePort"));
        }
        out.write(reinterpret_cast<const char*>(request.file_content.data()),
                  static_cast<std::streamsize>(request.file_content.size()));
        out.close();
        return Result<std::string>::Success(full_path.string());
    }

    Result<void> Delete(const std::string& stored_path) override {
        std::error_code ec;
        std::filesystem::remove(stored_path, ec);
        if (ec) {
            return Result<void>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "delete failed", "TestUploadStoragePort"));
        }
        return Result<void>::Success();
    }

   private:
    std::filesystem::path base_dir_;
};

class FakeUploadPreparationPort final : public IUploadPreparationPort {
   public:
    using EnsureHandler = std::function<Result<PreparedInputArtifact>(const std::string&)>;
    using CleanupHandler = std::function<Result<void>(const std::string&)>;

    FakeUploadPreparationPort(EnsureHandler ensure_handler = {}, CleanupHandler cleanup_handler = {})
        : ensure_handler_(std::move(ensure_handler)),
          cleanup_handler_(std::move(cleanup_handler)) {}

    Result<PreparedInputArtifact> EnsurePreparedInput(const std::string& source_path) const override {
        if (ensure_handler_) {
            return ensure_handler_(source_path);
        }

        const auto pb_path = PbPathFor(source_path);
        WriteTextFile(pb_path, "pb");
        PreparedInputArtifact artifact;
        artifact.prepared_path = pb_path.string();
        artifact.input_quality.classification = "success";
        artifact.input_quality.preview_ready = true;
        artifact.input_quality.production_ready = true;
        artifact.input_quality.summary = "DXF import succeeded and is ready for production.";
        artifact.input_quality.resolved_units = "mm";
        artifact.input_quality.resolved_unit_scale = 1.0;
        return Result<PreparedInputArtifact>::Success(std::move(artifact));
    }

    Result<void> CleanupPreparedInput(const std::string& source_path) const override {
        if (cleanup_handler_) {
            return cleanup_handler_(source_path);
        }

        std::error_code ec;
        std::filesystem::remove(PbPathFor(source_path), ec);
        if (ec) {
            return Result<void>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "cleanup failed", "FakeUploadPreparationPort"));
        }
        return Result<void>::Success();
    }

   private:
    EnsureHandler ensure_handler_;
    CleanupHandler cleanup_handler_;
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
