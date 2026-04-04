#pragma once

#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/Primitive.h"

#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::NormalizationConfig;
using Siligen::ProcessPath::Contracts::NormalizedPath;
using Siligen::ProcessPath::Contracts::Primitive;

class GeometryNormalizer {
   public:
    GeometryNormalizer() = default;
    ~GeometryNormalizer() = default;

    NormalizedPath Normalize(const std::vector<Primitive>& primitives,
                             const NormalizationConfig& config) const noexcept;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
