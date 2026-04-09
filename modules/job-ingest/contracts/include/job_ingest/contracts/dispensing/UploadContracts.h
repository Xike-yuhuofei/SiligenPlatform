#pragma once

#include "shared/types/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::JobIngest::Contracts {

using Siligen::Shared::Types::Result;

struct UploadRequest {
    std::vector<uint8_t> file_content;
    std::string original_filename;
    size_t file_size = 0;
    std::string content_type;

    [[nodiscard]] bool Validate() const {
        return !file_content.empty() && !original_filename.empty() && file_size > 0;
    }
};

struct UploadResponse {
    bool success = false;
    std::string filepath;
    std::string original_name;
    size_t size = 0;
    std::string generated_filename;
    int64_t timestamp = 0;
};

class IUploadFilePort {
   public:
    virtual ~IUploadFilePort() = default;

    virtual Result<UploadResponse> Execute(const UploadRequest& request) = 0;
};

}  // namespace Siligen::JobIngest::Contracts
