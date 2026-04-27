#pragma once

#include "process_path/contracts/PathPrimitiveMeta.h"
#include "process_path/contracts/Primitive.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::Result;

struct PathSourceResult {
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
};

class IPathSourcePort {
   public:
    virtual ~IPathSourcePort() = default;

    virtual Result<PathSourceResult> LoadFromFile(const std::string& filepath) = 0;
};

}  // namespace Siligen::ProcessPath::Contracts
