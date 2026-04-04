#pragma once

#include "../value-objects/Path.h"
#include "../value-objects/ProcessPath.h"
#include "../value-objects/ProcessConfig.h"

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Domain::Trajectory::ValueObjects::Path;
using Siligen::Domain::Trajectory::ValueObjects::ProcessConfig;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;

class ProcessAnnotator {
   public:
    ProcessAnnotator() = default;
    ~ProcessAnnotator() = default;

    ProcessPath Annotate(const Path& path, const ProcessConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
