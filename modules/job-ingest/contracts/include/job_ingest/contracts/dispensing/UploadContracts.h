#pragma once

#include "engineering/contracts/DxfValidationReport.h"
#include "shared/types/Result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::JobIngest::Contracts {

using Siligen::Engineering::Contracts::DxfValidationReport;
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

struct SourceDrawing {
    std::string source_drawing_ref;
    std::string filepath;
    std::string original_name;
    size_t size = 0;
    std::string generated_filename;
    int64_t timestamp = 0;
    std::string source_hash;
    DxfValidationReport validation_report;
};

class IUploadFilePort {
   public:
    virtual ~IUploadFilePort() = default;

    virtual Result<SourceDrawing> Execute(const UploadRequest& request) = 0;
};

}  // namespace Siligen::JobIngest::Contracts
