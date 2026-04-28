#pragma once

#include "job_ingest/application/ports/dispensing/UploadPorts.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::JobIngest::Application::UseCases::Dispensing {

using Siligen::JobIngest::Application::Ports::Dispensing::IUploadStoragePort;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::SourceDrawing;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::Shared::Types::Result;

class UploadFileUseCase : public IUploadFilePort {
   public:
    explicit UploadFileUseCase(std::shared_ptr<IUploadStoragePort> storage_port,
                               size_t max_file_size_mb = 10);

    ~UploadFileUseCase() override = default;

    UploadFileUseCase(const UploadFileUseCase&) = delete;
    UploadFileUseCase& operator=(const UploadFileUseCase&) = delete;
    UploadFileUseCase(UploadFileUseCase&&) = delete;
    UploadFileUseCase& operator=(UploadFileUseCase&&) = delete;

    Result<SourceDrawing> Execute(const UploadRequest& request) override;

   private:
    std::shared_ptr<IUploadStoragePort> storage_port_;
    size_t max_file_size_mb_;

    std::string GenerateSafeFilename(const std::string& original_filename);
    Result<std::string> ValidateStoredDxfArtifact(
        const UploadRequest& request,
        const std::string& source_drawing_ref,
        const std::string& source_hash) const;
    std::string ComputeSourceHash(const UploadRequest& request) const;
    void CleanupStoredArtifact(const std::string& stored_path) const noexcept;
};

}  // namespace Siligen::JobIngest::Application::UseCases::Dispensing
