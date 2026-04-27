#pragma once

#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "shared/types/Result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace Siligen::JobIngest::Application::Ports::Dispensing {

using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::Shared::Types::Result;

class IUploadStoragePort {
   public:
    virtual ~IUploadStoragePort() = default;

    virtual Result<void> Validate(const UploadRequest& request,
                                  size_t max_file_size_mb,
                                  const std::vector<std::string>& allowed_extensions) const = 0;

    virtual Result<std::string> Store(const UploadRequest& request, const std::string& target_filename) = 0;

    virtual Result<void> Delete(const std::string& stored_path) = 0;
};

}  // namespace Siligen::JobIngest::Application::Ports::Dispensing
