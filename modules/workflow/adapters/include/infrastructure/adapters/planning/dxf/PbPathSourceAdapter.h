#pragma once

#include "domain/trajectory/ports/IPathSourcePort.h"

#include <string>

namespace Siligen::Infrastructure::Adapters::Parsing {

using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::Shared::Types::Result;

class PbPathSourceAdapter : public Domain::Trajectory::Ports::IPathSourcePort {
   public:
    PbPathSourceAdapter() = default;
    ~PbPathSourceAdapter() override = default;

    Result<PathSourceResult> LoadFromFile(const std::string& filepath) override;
};

}  // namespace Siligen::Infrastructure::Adapters::Parsing
