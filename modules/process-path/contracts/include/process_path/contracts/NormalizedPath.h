#pragma once

#include "process_path/contracts/NormalizationConfig.h"
#include "process_path/contracts/Path.h"

namespace Siligen::ProcessPath::Contracts {

struct NormalizedPath {
    Path path;
    NormalizationReport report;
};

}  // namespace Siligen::ProcessPath::Contracts
