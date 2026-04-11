#pragma once

#include "coordinate_alignment/contracts/CoordinateTransformSet.h"
#include "process_path/contracts/PathPrimitiveMeta.h"
#include "process_path/contracts/NormalizationConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/TopologyRepairConfig.h"
#include "process_path/contracts/TrajectoryShaperConfig.h"

#include <optional>
#include <vector>

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationRequest {
    std::vector<Primitive> primitives;
    // metadata is required 1:1 with primitives when topology_repair.policy == Auto.
    std::vector<PathPrimitiveMeta> metadata;
    // alignment is provenance from M5, not a live geometric transform in M6.
    std::optional<Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet> alignment;
    NormalizationConfig normalization{};
    ProcessConfig process{};
    TopologyRepairConfig topology_repair{};
    TrajectoryShaperConfig shaping{};
};

}  // namespace Siligen::ProcessPath::Contracts
