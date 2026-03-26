#pragma once

#include "domain/trajectory/value-objects/ProcessPath.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Trajectory::DomainServices {

using Siligen::Shared::Types::float32;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;

struct TrajectoryShaperConfig {
    float32 corner_smoothing_radius = 0.5f;
    float32 corner_max_deviation_mm = 0.1f;
    float32 corner_min_radius_mm = 0.1f;
    float32 position_tolerance = 0.1f;
};

class TrajectoryShaper {
   public:
    TrajectoryShaper() = default;
    ~TrajectoryShaper() = default;

    ProcessPath Shape(const ProcessPath& input, const TrajectoryShaperConfig& config) const;
};

}  // namespace Siligen::Domain::Trajectory::DomainServices
