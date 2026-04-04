#pragma once

#include "shared/types/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::JobIngest::Contracts::Storage {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::Result;

struct FileData {
    std::vector<uint8_t> content;
    std::string original_name;
    size_t size;
    std::string content_type;
};

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

}  // namespace Siligen::JobIngest::Contracts::Storage
