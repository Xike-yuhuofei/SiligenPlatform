#pragma once

#include "domain/configuration/ports/IFileStoragePort.h"
#include "shared/types/Result.h"
#include <functional>
#include <memory>
#include <vector>
#include <string>

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

/**
 * @brief 清理 DXF 文件请求参数
 */
struct CleanupDXFFilesRequest {
    uint32_t max_age_hours = 24;  ///< 最大保留时间（小时）
    bool dry_run = false;          ///< 是否仅模拟清理（不实际删除）

    /**
     * @brief 验证请求参数
     */
    Result<void> Validate() const noexcept {
        if (max_age_hours == 0) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "max_age_hours must be greater than 0"));
        }
        return Result<void>::Success();
    }
};

/**
 * @brief 清理 DXF 文件结果
 */
struct CleanupDXFFilesResult {
    bool success = false;
    uint32_t deleted_count = 0;
    float freed_space_mb = 0.0f;
    std::vector<std::string> deleted_files;
    std::string message;
};

/**
 * @brief 清理 DXF 文件用例
 *
 * 职责：
 * 1. 列出上传目录中的所有 DXF 文件
 * 2. 检查文件是否过期（基于最后修改时间）
 * 3. 删除过期文件（或模拟删除）
 * 4. 返回清理统计信息
 */
class CleanupDXFFilesUseCase {
public:
    using FileInUseChecker = std::function<bool(const std::string&)>;

    /**
     * @brief 构造函数
     * @param file_storage_port 文件存储端口
     */
    explicit CleanupDXFFilesUseCase(
        std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port,
        std::string base_directory = "uploads/dxf/",
        FileInUseChecker file_in_use_checker = {});

    ~CleanupDXFFilesUseCase() = default;

    // 禁止拷贝和移动
    CleanupDXFFilesUseCase(const CleanupDXFFilesUseCase&) = delete;
    CleanupDXFFilesUseCase& operator=(const CleanupDXFFilesUseCase&) = delete;

    /**
     * @brief 执行清理流程
     * @param request 请求参数
     * @return 清理结果
     */
    Result<CleanupDXFFilesResult> Execute(const CleanupDXFFilesRequest& request);

private:
    std::shared_ptr<Domain::Configuration::Ports::IFileStoragePort> file_storage_port_;
    std::string base_directory_;
    FileInUseChecker file_in_use_checker_;

    /**
     * @brief 获取上传目录中的所有 DXF 文件
     */
    Result<std::vector<std::string>> ListDXFFiles() noexcept;

    /**
     * @brief 检查文件是否过期
     * @param filepath 文件路径
     * @param max_age_hours 最大保留时间（小时）
     */
    bool IsFileExpired(const std::string& filepath, uint32_t max_age_hours) noexcept;

    /**
     * @brief 检查文件是否正在使用
     * @param filepath 文件路径
     * @return true 如果正在使用，false 否则
     *
     * 当前优先预留可注入检测抽象，避免直接耦合平台文件锁。
     */
    bool IsFileInUse(const std::string& filepath) noexcept;
};

}  // namespace Siligen::Application::UseCases::Dispensing::DXF




