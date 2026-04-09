#pragma once

#include "process_path/contracts/IPathSourcePort.h"

#include <string>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::ProcessPath::Contracts::PathSourceResult;
using Siligen::Shared::Types::Result;

class PbPathSourceAdapter : public ProcessPath::Contracts::IPathSourcePort {
   public:
    PbPathSourceAdapter() = default;
    ~PbPathSourceAdapter() override = default;

    Result<PathSourceResult> LoadFromFile(const std::string& filepath) override;
};

}  // namespace Siligen::Infrastructure::Adapters::Parsing
