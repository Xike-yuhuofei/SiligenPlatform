#pragma once

#include "shared/types/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Siligen::Application::Services::DXF::detail {

using Siligen::Shared::Types::Result;

class PbProcessRunner {
   public:
    Result<void> Run(const std::vector<std::string>& command_args, const std::filesystem::path& pb_path) const;
    Result<void> ValidateOutput(const std::filesystem::path& pb_path) const;
};

}  // namespace Siligen::Application::Services::DXF::detail
