#pragma once

#include <memory>
#include <string>

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Shared::Types {
template <typename T>
class Result;
}

namespace Siligen::Application::Services::DXF {

using Siligen::Shared::Types::Result;

class DxfPbPreparationService {
   public:
    explicit DxfPbPreparationService(
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port = nullptr);

    Result<std::string> EnsurePbReady(const std::string& filepath) const;
    Result<void> CleanupPreparedInput(const std::string& source_path) const;

   private:
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
};

}  // namespace Siligen::Application::Services::DXF
