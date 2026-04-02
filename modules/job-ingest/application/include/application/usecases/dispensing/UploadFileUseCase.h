#pragma once

#include "job_ingest/contracts/dispensing/UploadContracts.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}
namespace Siligen::JobIngest::Contracts::Storage {
class IFileStoragePort;
}
namespace Siligen::Application::Services::DXF {
class DxfPbPreparationService;
}
namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::Application::UseCases::Dispensing {

using Siligen::Shared::Types::Result;

class UploadFileUseCase : public IUploadFilePort {
   public:
    UploadFileUseCase(std::shared_ptr<JobIngest::Contracts::Storage::IFileStoragePort> file_storage_port,
                      size_t max_file_size_mb = 10,
                      std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr,
                      std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService>
                          pb_preparation_service = nullptr);

    ~UploadFileUseCase() = default;

    UploadFileUseCase(const UploadFileUseCase&) = delete;
    UploadFileUseCase& operator=(const UploadFileUseCase&) = delete;
    UploadFileUseCase(UploadFileUseCase&&) = delete;
    UploadFileUseCase& operator=(UploadFileUseCase&&) = delete;

    Result<UploadResponse> Execute(const UploadRequest& request) override;

   private:
    std::shared_ptr<JobIngest::Contracts::Storage::IFileStoragePort> file_storage_port_;
    size_t max_file_size_mb_;
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service_;

    std::string GenerateSafeFilename(const std::string& original_filename);
    Result<void> ValidateFileFormat(const std::vector<uint8_t>& file_content);
    void CleanupGeneratedArtifacts(const std::string& filepath) noexcept;
};

}  // namespace Siligen::Application::UseCases::Dispensing
