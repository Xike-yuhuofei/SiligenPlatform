#pragma once

#include "shared/types/Result.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Domain::Configuration::Ports {
class IConfigurationPort;
}

namespace Siligen::Application::Services::DXF::detail {

using Siligen::Shared::Types::Result;

class PbCommandResolver {
   public:
    explicit PbCommandResolver(
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

    Result<std::vector<std::string>> Resolve(const std::filesystem::path& dxf_path,
                                             const std::filesystem::path& pb_path) const;

   private:
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
};

}  // namespace Siligen::Application::Services::DXF::detail
