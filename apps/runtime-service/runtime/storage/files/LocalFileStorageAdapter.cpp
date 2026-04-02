// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "LocalFileStorage"

#include "LocalFileStorageAdapter.h"

#include "shared/types/Result.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <fstream>
#include <system_error>

#include "shared/interfaces/ILoggingService.h"

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

LocalFileStorageAdapter::LocalFileStorageAdapter(const std::string& base_directory) : base_directory_(base_directory) {
    // 确保基础目录存在
    auto result = EnsureDirectoryExists(base_directory_);
    if (!result.IsSuccess()) {
        // Cannot throw - caller should check EnsureDirectoryExists result before using
    }
}

Result<void> LocalFileStorageAdapter::ValidatePathSafety(const std::string& filepath) {
    // 防止路径遍历攻击
    std::filesystem::path path_obj(filepath);

    // 规范化路径（解析 .. 和 .）
    std::error_code ec;
    auto canonical_base = std::filesystem::canonical(base_directory_, ec);
    if (ec) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_PATH_INVALID, "Invalid base directory: " + base_directory_, "LocalFileStorageAdapter"));
    }

    auto target_parent = std::filesystem::canonical(path_obj.parent_path(), ec);

    if (ec) {
        // 如果父目录不存在（文件未创建），使用基础目录
        target_parent = canonical_base;
    }

    // 确保目标路径在基础目录内
    auto [base_it, target_it] = std::mismatch(canonical_base.begin(), canonical_base.end(), target_parent.begin());

    if (base_it != canonical_base.end()) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_PATH_INVALID, "Path traversal attempt detected: " + filepath, "LocalFileStorageAdapter"));
    }

    return Result<void>::Success();
}

Result<void> LocalFileStorageAdapter::CheckDiskSpace(size_t required_bytes) {
    std::error_code ec;
    auto space_info = std::filesystem::space(base_directory_, ec);

    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::DISK_SPACE_INSUFFICIENT,
                                           "Failed to check disk space: " + ec.message(),
                                           "LocalFileStorageAdapter"));
    }

    if (space_info.available < required_bytes) {
        return Result<void>::Failure(
            Error(ErrorCode::DISK_SPACE_INSUFFICIENT,
                  "Insufficient disk space. Available: " + std::to_string(space_info.available / 1024 / 1024) +
                      " MB, " + "Required: " + std::to_string(required_bytes / 1024 / 1024) + " MB",
                  "LocalFileStorageAdapter"));
    }

    return Result<void>::Success();
}

std::string LocalFileStorageAdapter::NormalizePath(const std::string& filename) {
    // 移除路径中的危险字符，只保留文件名部分
    std::string safe_filename = filename;

    // 移除路径分隔符（只保留文件名部分）
    size_t last_slash = safe_filename.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        safe_filename = safe_filename.substr(last_slash + 1);
    }

    // 组合完整路径
    std::filesystem::path full_path = std::filesystem::path(base_directory_) / safe_filename;

    return full_path.string();
}

Result<void> LocalFileStorageAdapter::EnsureDirectoryExists(const std::string& directory) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);

    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR,
                                           "Failed to create directory: " + directory + " - " + ec.message(),
                                           "LocalFileStorageAdapter"));
    }

    return Result<void>::Success();
}

Result<void> LocalFileStorageAdapter::DeleteFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(file_mutex_);

    // 验证路径安全
    auto safety_result = ValidatePathSafety(filepath);
    if (!safety_result.IsSuccess()) {
        return Result<void>::Failure(safety_result.GetError());
    }

    std::error_code ec;
    std::filesystem::remove(filepath, ec);

    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::FILE_DELETE_FAILED,
                                           "Failed to delete file: " + filepath + " - " + ec.message(),
                                           "LocalFileStorageAdapter"));
    }

    SILIGEN_LOG_INFO(std::string("File deleted: ") + filepath);
    return Result<void>::Success();
}

Result<bool> LocalFileStorageAdapter::FileExists(const std::string& filepath) {
    std::error_code ec;
    bool exists = std::filesystem::exists(filepath, ec);
    if (ec) {
        return Result<bool>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "File exists check failed: " + ec.message(), "LocalFileStorageAdapter"));
    }
    return Result<bool>::Success(exists);
}


Result<std::string> LocalFileStorageAdapter::StoreFile(const Domain::Configuration::Ports::FileData& file_data,
                                                       const std::string& filename) {
    std::lock_guard<std::mutex> lock(file_mutex_);

    // 验证文件数据
    if (file_data.content.empty()) {
        return Result<std::string>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "File data is empty", "LocalFileStorageAdapter"));
    }

    // 规范化路径
    std::string safe_filename = filename.empty() ? file_data.original_name : filename;
    std::string full_path = NormalizePath(safe_filename);

    // 验证路径安全
    auto safety_result = ValidatePathSafety(full_path);
    if (!safety_result.IsSuccess()) {
        return Result<std::string>::Failure(safety_result.GetError());
    }

    
        // 确保目录存在
        auto parent_dir = std::filesystem::path(full_path).parent_path();
        if (!parent_dir.empty()) {
            auto dir_result = EnsureDirectoryExists(parent_dir.string());
            if (!dir_result.IsSuccess()) {
                return Result<std::string>::Failure(dir_result.GetError());
            }
        }

        // 写入文件
        std::ofstream outfile(full_path, std::ios::binary);
        if (!outfile) {
            return Result<std::string>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "Failed to create file: " + full_path, "LocalFileStorageAdapter"));
        }

        outfile.write(reinterpret_cast<const char*>(file_data.content.data()), file_data.content.size());

        if (!outfile) {
            return Result<std::string>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "Failed to write file: " + full_path, "LocalFileStorageAdapter"));
        }

        outfile.close();

        SILIGEN_LOG_INFO("File stored: " + full_path + " (" + std::to_string(file_data.content.size()) + " bytes)");

        return Result<std::string>::Success(full_path);

}

Result<std::string> LocalFileStorageAdapter::StoreFile(
    const JobIngest::Contracts::Storage::FileData& file_data,
    const std::string& filename) {
    Domain::Configuration::Ports::FileData normalized{
        file_data.content,
        file_data.original_name,
        file_data.size,
        file_data.content_type};
    return StoreFile(normalized, filename);
}

Result<void> LocalFileStorageAdapter::ValidateFile(const Domain::Configuration::Ports::FileData& file_data,
                                                   size_t max_size_mb,
                                                   const std::vector<std::string>& allowed_extensions) {
    // 检查文件数据
    if (file_data.content.empty()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "File data is empty", "LocalFileStorageAdapter"));
    }

    // 检查文件大小
    size_t max_size_bytes = max_size_mb * 1024 * 1024;
    if (file_data.size > max_size_bytes) {
        return Result<void>::Failure(Error(ErrorCode::FILE_SIZE_EXCEEDED,
                                           "File size " + std::to_string(file_data.size) + " bytes exceeds maximum " +
                                               std::to_string(max_size_bytes) + " bytes",
                                           "LocalFileStorageAdapter"));
    }

    // 检查文件扩展名
    if (!allowed_extensions.empty()) {
        auto NormalizeExtension = [](std::string ext) {
            if (!ext.empty() && ext[0] != '.') {
                ext.insert(ext.begin(), '.');
            }
            // 使用 lambda 包装 tolower，避免 UTF-8 字符导致的负值问题
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
            return ext;
        };

        size_t dot_pos = file_data.original_name.find_last_of('.');
        if (dot_pos == std::string::npos || dot_pos + 1 >= file_data.original_name.size()) {
            return Result<void>::Failure(Error(ErrorCode::FILE_TYPE_INVALID,
                                               "File extension is missing",
                                               "LocalFileStorageAdapter"));
        }

        std::string extension = file_data.original_name.substr(dot_pos);
        std::string normalized_extension = NormalizeExtension(extension);

        bool is_allowed = false;
        for (const auto& ext : allowed_extensions) {
            if (normalized_extension == NormalizeExtension(ext)) {
                is_allowed = true;
                break;
            }
        }

        if (!is_allowed) {
            return Result<void>::Failure(Error(ErrorCode::FILE_TYPE_INVALID,
                                               "File extension '" + normalized_extension + "' is not allowed",
                                               "LocalFileStorageAdapter"));
        }
    }

    // 检查文件内容完整性
    if (file_data.content.size() != file_data.size) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "File content size mismatch", "LocalFileStorageAdapter"));
    }

    return Result<void>::Success();
}

Result<void> LocalFileStorageAdapter::ValidateFile(
    const JobIngest::Contracts::Storage::FileData& file_data,
    size_t max_size_mb,
    const std::vector<std::string>& allowed_extensions) {
    Domain::Configuration::Ports::FileData normalized{
        file_data.content,
        file_data.original_name,
        file_data.size,
        file_data.content_type};
    return ValidateFile(normalized, max_size_mb, allowed_extensions);
}

Result<size_t> LocalFileStorageAdapter::GetFileSize(const std::string& filepath) {
    std::error_code ec;
    auto size = std::filesystem::file_size(filepath, ec);

    if (ec) {
        return Result<size_t>::Failure(Error(ErrorCode::FILE_NOT_FOUND,
                                             "Failed to get file size: " + filepath + " - " + ec.message(),
                                             "LocalFileStorageAdapter"));
    }

    return Result<size_t>::Success(size);
}

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen

