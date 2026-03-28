#pragma once

#include "coordinate_alignment/contracts/CoordinateTransformSet.h"
#include "process_path/contracts/NormalizationConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/TrajectoryShaperConfig.h"

#include <optional>
#include <vector>

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationRequest {
    std::vector<Primitive> primitives;
    std::optional<Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet> alignment;
    NormalizationConfig normalization{};
    ProcessConfig process{};
    TrajectoryShaperConfig shaping{};
};

}  // namespace Siligen::ProcessPath::Contracts
