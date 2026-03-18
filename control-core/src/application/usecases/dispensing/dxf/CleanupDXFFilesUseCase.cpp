#include "CleanupDXFFilesUseCase.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <cctype>
#include <filesystem>

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "CleanupDXFFilesUseCase"

namespace Siligen::Application::UseCases::Dispensing::DXF {

CleanupDXFFilesUseCase::CleanupDXFFilesUseCase(
    std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port,
    std::string base_directory,
    FileInUseChecker file_in_use_checker)
    : file_storage_port_(std::move(file_storage_port)),
      base_directory_(std::move(base_directory)),
      file_in_use_checker_(std::move(file_in_use_checker)) {}

Result<CleanupDXFFilesResult> CleanupDXFFilesUseCase::Execute(
    const CleanupDXFFilesRequest& request) {

    // 1. 验证请求
    auto validation = request.Validate();
    if (!validation.IsSuccess()) {
        return Result<CleanupDXFFilesResult>::Failure(validation.GetError());
    }

    // 2. 列出所有 DXF 文件
    auto files_result = ListDXFFiles();
    if (!files_result.IsSuccess()) {
        return Result<CleanupDXFFilesResult>::Failure(files_result.GetError());
    }
    auto files = files_result.Value();

    CleanupDXFFilesResult result;
    result.success = true;

    // 3. 遍历文件，检查是否过期
    for (const auto& filepath : files) {
        // 检查是否过期
        if (!IsFileExpired(filepath, request.max_age_hours)) {
            continue;
        }

        // 检查是否正在使用
        if (IsFileInUse(filepath)) {
            continue;
        }

        // 获取文件大小
        auto size_result = file_storage_port_->GetFileSize(filepath);
        if (size_result.IsSuccess()) {
            result.freed_space_mb += size_result.Value() / (1024.0f * 1024.0f);
        }

        // 删除文件（如果不是 dry_run）
        if (!request.dry_run) {
            auto delete_result = file_storage_port_->DeleteFile(filepath);
            if (!delete_result.IsSuccess()) {
                // 记录错误但继续清理
                SILIGEN_LOG_WARNING("Failed to delete file: " + filepath + " - " + delete_result.GetError().GetMessage());
                continue;
            }
        }

        result.deleted_count++;
        result.deleted_files.push_back(filepath);
    }

    result.message = "Cleanup completed: " + std::to_string(result.deleted_count) + " files deleted";
    if (request.dry_run) {
        result.message += " (dry run mode)";
    }

    return Result<CleanupDXFFilesResult>::Success(result);
}

Result<std::vector<std::string>> CleanupDXFFilesUseCase::ListDXFFiles() noexcept {
    std::vector<std::string> files;

    std::error_code ec;
    if (!std::filesystem::exists(base_directory_, ec)) {
        return Result<std::vector<std::string>>::Success(files);
    }
    if (ec) {
        return Result<std::vector<std::string>>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Failed to access directory: " + base_directory_));
    }

    for (const auto& entry : std::filesystem::directory_iterator(base_directory_, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        auto ext = entry.path().extension().string();
        for (auto& ch : ext) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == ".dxf") {
            files.push_back(entry.path().string());
        }
    }

    return Result<std::vector<std::string>>::Success(files);
}

bool CleanupDXFFilesUseCase::IsFileExpired(const std::string& filepath, uint32_t max_age_hours) noexcept {
    std::error_code ec;
    auto file_time = std::filesystem::last_write_time(filepath, ec);
    if (ec) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto file_sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() + now);

    auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - file_sys_time).count();
    int64_t max_age_seconds = static_cast<int64_t>(max_age_hours) * 3600;

    return age_seconds > max_age_seconds;
}

bool CleanupDXFFilesUseCase::IsFileInUse(const std::string& filepath) noexcept {
    if (file_in_use_checker_) {
        return file_in_use_checker_(filepath);
    }

    (void)filepath;
    return false;
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF


