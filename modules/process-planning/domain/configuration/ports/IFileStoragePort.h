#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Forward declarations
namespace Siligen::Shared::Types {
template <typename T = void>
class Result;
class Error;
}  // namespace Siligen::Shared::Types

namespace Siligen {
namespace Domain {
namespace Configuration {
namespace Ports {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::Result;

/**
 * @brief 文件数据结构
 *
 * 用于封装上传文件的所有相关信息
 */
struct FileData {
    std::vector<uint8_t> content;  ///< 文件内容
    std::string original_name;     ///< 原始文件名
    size_t size;                   ///< 文件大小（字节）
    std::string content_type;      ///< MIME 类型
};

/**
 * @brief 文件存储端口接口
 *
 * 职责:
 * - 定义文件存储抽象
 * - 不包含具体实现
 * - 由 Infrastructure 层实现
 *
 * 架构合规性:
 * - Domain 层零 IO 依赖
 * - 纯虚接口，无实现
 * - 使用 Result<T> 错误处理
 */
class IFileStoragePort {
   public:
    virtual ~IFileStoragePort() = default;

    /**
     * @brief 存储文件
     * @param file_data 文件数据
     * @param filename 目标文件名
     * @return Result<std::string> 成功返回完整文件路径
     */
    virtual Result<std::string> StoreFile(const FileData& file_data, const std::string& filename) = 0;

    /**
     * @brief 验证文件
     * @param file_data 文件数据
     * @param max_size_mb 最大文件大小（MB）
     * @param allowed_extensions 允许的扩展名
     * @return Result<void> 验证结果
     */
    virtual Result<void> ValidateFile(const FileData& file_data,
                                      size_t max_size_mb,
                                      const std::vector<std::string>& allowed_extensions) = 0;

    /**
     * @brief 删除文件
     * @param filepath 文件路径
     * @return Result<void> 删除结果
     */
    virtual Result<void> DeleteFile(const std::string& filepath) = 0;

    /**
     * @brief 检查文件是否存在
     * @param filepath 文件路径
     * @return Result<bool> 成功返回文件存在状态，失败返回错误信息
     *         - ErrorCode::FILE_NOT_FOUND 文件不存在
     *         - ErrorCode::IO_ERROR 文件系统IO错误
     */
    virtual Result<bool> FileExists(const std::string& filepath) = 0;

    /**
     * @brief 获取文件大小
     * @param filepath 文件路径
     * @return Result<size_t> 文件大小（字节）
     */
    virtual Result<size_t> GetFileSize(const std::string& filepath) = 0;
};

}  // namespace Ports
}  // namespace Configuration
}  // namespace Domain
}  // namespace Siligen
