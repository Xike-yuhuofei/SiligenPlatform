#pragma once

#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/TrajectoryShaperConfig.h"

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::TrajectoryShaperConfig;

class TrajectoryShaper {
   public:
    TrajectoryShaper() = default;
    ~TrajectoryShaper() = default;

    ProcessPath Shape(const ProcessPath& input, const TrajectoryShaperConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
