#pragma once

#include "shared/types/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Configuration {
namespace Ports {

using Siligen::Shared::Types::Result;

/**
 * @brief 文件数据结构
 *
 * runtime-service bootstrap app-local storage seam 使用的最小文件载荷。
 */
struct FileData {
    std::vector<uint8_t> content;
    std::string original_name;
    size_t size;
    std::string content_type;
};

/**
 * @brief runtime-service bootstrap app-local 文件存储端口
 *
 * 该 seam 只承接 host-local upload storage wiring，不再由 process-planning 持有。
 */
class IFileStoragePort {
   public:
    virtual ~IFileStoragePort() = default;

    virtual Result<std::string> StoreFile(const FileData& file_data, const std::string& filename) = 0;

    virtual Result<void> ValidateFile(const FileData& file_data,
                                      size_t max_size_mb,
                                      const std::vector<std::string>& allowed_extensions) = 0;

    virtual Result<void> DeleteFile(const std::string& filepath) = 0;

    virtual Result<bool> FileExists(const std::string& filepath) = 0;

    virtual Result<size_t> GetFileSize(const std::string& filepath) = 0;
};

}  // namespace Ports
}  // namespace Configuration
}  // namespace Domain
}  // namespace Siligen
