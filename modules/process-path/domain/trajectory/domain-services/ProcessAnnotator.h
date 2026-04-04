#pragma once

#include "process_path/contracts/Path.h"
#include "process_path/contracts/ProcessConfig.h"
#include "process_path/contracts/ProcessPath.h"

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::Path;
using Siligen::ProcessPath::Contracts::ProcessConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;

class ProcessAnnotator {
   public:
    ProcessAnnotator() = default;
    ~ProcessAnnotator() = default;

    ProcessPath Annotate(const Path& path, const ProcessConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
