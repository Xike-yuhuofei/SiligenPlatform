#pragma once

#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationResult {
    NormalizedPath normalized;
    ProcessPath process_path;
    ProcessPath shaped_path;
};

}  // namespace Siligen::ProcessPath::Contracts
