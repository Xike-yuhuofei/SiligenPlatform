#pragma once

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "domain/trajectory/value-objects/Primitive.h"

#include <vector>

namespace Siligen::Application::Services::ProcessPath {

using Siligen::Domain::Trajectory::DomainServices::NormalizationConfig;
using Siligen::Domain::Trajectory::DomainServices::NormalizedPath;
using Siligen::Domain::Trajectory::DomainServices::TrajectoryShaperConfig;
using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Domain::Trajectory::ValueObjects::ProcessConfig;
using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;

struct ProcessPathBuildRequest {
    std::vector<Primitive> primitives;
    NormalizationConfig normalization{};
    ProcessConfig process{};
    TrajectoryShaperConfig shaping{};
};

struct ProcessPathBuildResult {
    NormalizedPath normalized;
    ProcessPath process_path;
    ProcessPath shaped_path;
};

class ProcessPathFacade {
   public:
    ProcessPathBuildResult Build(const ProcessPathBuildRequest& request) const;
};

}  // namespace Siligen::Application::Services::ProcessPath
