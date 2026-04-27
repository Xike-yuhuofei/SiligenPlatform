#pragma once

#include "runtime_process_bootstrap/storage/ports/IFileStoragePort.h"
#include "shared/types/Result.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using Siligen::Shared::Types::Result;

/**
 * @brief 本地文件存储适配器
 *
 * 实现 runtime-service bootstrap app-local IFileStoragePort 接口
 * - 管理本地文件系统
 * - 线程安全文件操作
 * - 路径遍历攻击防护
 *
 * 架构合规性:
 * - 实现 Domain 层端口接口
 * - 依赖注入到 Application 层
 * - 使用 Result<T> 错误处理
 */
class LocalFileStorageAdapter
    : public Domain::Configuration::Ports::IFileStoragePort {
   public:
    /**
     * @brief 构造函数
     * @param base_directory 基础存储目录
     */
    explicit LocalFileStorageAdapter(const std::string& base_directory);

    ~LocalFileStorageAdapter() override = default;

    // 禁止拷贝和移动
    LocalFileStorageAdapter(const LocalFileStorageAdapter&) = delete;
    LocalFileStorageAdapter& operator=(const LocalFileStorageAdapter&) = delete;
    LocalFileStorageAdapter(LocalFileStorageAdapter&&) = delete;
    LocalFileStorageAdapter& operator=(LocalFileStorageAdapter&&) = delete;

    // 实现 IFileStoragePort 接口
    Result<std::string> StoreFile(const Domain::Configuration::Ports::FileData& file_data, const std::string& filename) override;

    Result<void> ValidateFile(const Domain::Configuration::Ports::FileData& file_data,
                              size_t max_size_mb,
                              const std::vector<std::string>& allowed_extensions) override;

    Result<void> DeleteFile(const std::string& filepath) override;

    Result<bool> FileExists(const std::string& filepath) override;

    Result<size_t> GetFileSize(const std::string& filepath) override;

   private:
    std::string base_directory_;
    std::mutex file_mutex_;  // 线程安全文件操作

    /**
     * @brief 验证路径安全性（防止路径遍历攻击）
     * @param filepath 文件路径
     * @return Result<void> 验证结果
     */
    Result<void> ValidatePathSafety(const std::string& filepath);

    /**
     * @brief 检查磁盘空间
     * @param required_bytes 需要的字节数
     * @return Result<void> 检查结果
     */
    Result<void> CheckDiskSpace(size_t required_bytes);

    /**
     * @brief 规范化文件路径
     * @param filename 文件名
     * @return std::string 规范化后的路径
     */
    std::string NormalizePath(const std::string& filename);

    /**
     * @brief 确保目录存在
     * @param directory 目录路径
     * @return Result<void> 创建结果
     */
    static Result<void> EnsureDirectoryExists(const std::string& directory);
};

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen


