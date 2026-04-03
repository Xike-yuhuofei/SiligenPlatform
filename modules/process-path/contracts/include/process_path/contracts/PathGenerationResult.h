#pragma once

#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/PathTopologyDiagnostics.h"

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationResult {
    NormalizedPath normalized;
    ProcessPath process_path;
    ProcessPath shaped_path;
    PathTopologyDiagnostics topology_diagnostics;
};

}  // namespace Siligen::ProcessPath::Contracts
