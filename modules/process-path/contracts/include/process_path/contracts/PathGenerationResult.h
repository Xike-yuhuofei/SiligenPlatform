#pragma once

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationResult {
    Siligen::Domain::Trajectory::DomainServices::NormalizedPath normalized;
    ProcessPath process_path;
    ProcessPath shaped_path;
};

}  // namespace Siligen::ProcessPath::Contracts
