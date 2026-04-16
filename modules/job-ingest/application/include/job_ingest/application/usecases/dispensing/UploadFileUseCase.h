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

using Siligen::JobIngest::Application::Ports::Dispensing::IUploadPreparationPort;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadStoragePort;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::JobIngest::Contracts::UploadResponse;
using Siligen::Shared::Types::Result;

class UploadFileUseCase : public IUploadFilePort {
   public:
    UploadFileUseCase(std::shared_ptr<IUploadStoragePort> storage_port,
                      std::shared_ptr<IUploadPreparationPort> preparation_port,
                      size_t max_file_size_mb = 10);

    ~UploadFileUseCase() = default;

    UploadFileUseCase(const UploadFileUseCase&) = delete;
    UploadFileUseCase& operator=(const UploadFileUseCase&) = delete;
    UploadFileUseCase(UploadFileUseCase&&) = delete;
    UploadFileUseCase& operator=(UploadFileUseCase&&) = delete;

    Result<UploadResponse> Execute(const UploadRequest& request) override;

   private:
    std::shared_ptr<IUploadStoragePort> storage_port_;
    std::shared_ptr<IUploadPreparationPort> preparation_port_;
    size_t max_file_size_mb_;

    std::string GenerateSafeFilename(const std::string& original_filename);
    void CleanupGeneratedArtifacts(const std::string& stored_path) const noexcept;
};

}  // namespace Siligen::JobIngest::Application::UseCases::Dispensing
