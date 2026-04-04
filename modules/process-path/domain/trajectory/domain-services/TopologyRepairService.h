#pragma once

#include "process_path/contracts/PathPrimitiveMeta.h"
#include "process_path/contracts/PathTopologyDiagnostics.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/TopologyRepairConfig.h"
#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::PathTopologyDiagnostics;
using Siligen::ProcessPath::Contracts::TopologyRepairConfig;
using Siligen::Shared::Types::float32;

using ContractsPrimitive = Siligen::ProcessPath::Contracts::Primitive;

struct TopologyRepairResult {
    std::vector<ContractsPrimitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
    PathTopologyDiagnostics diagnostics;
};

class TopologyRepairService {
   public:
    TopologyRepairResult Repair(const std::vector<ContractsPrimitive>& primitives,
                                const std::vector<PathPrimitiveMeta>& metadata,
                                float32 continuity_tolerance_mm,
                                const TopologyRepairConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
