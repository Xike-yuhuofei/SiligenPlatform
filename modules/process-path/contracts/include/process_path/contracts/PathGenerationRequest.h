#pragma once

#include "coordinate_alignment/contracts/CoordinateTransformSet.h"
#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "process_path/contracts/ProcessPath.h"

#include <optional>
#include <vector>

namespace Siligen::ProcessPath::Contracts {

struct PathGenerationRequest {
    std::vector<Primitive> primitives;
    std::optional<Siligen::CoordinateAlignment::Contracts::CoordinateTransformSet> alignment;
    Siligen::Domain::Trajectory::DomainServices::NormalizationConfig normalization{};
    ProcessConfig process{};
    Siligen::Domain::Trajectory::DomainServices::TrajectoryShaperConfig shaping{};
};

}  // namespace Siligen::ProcessPath::Contracts
