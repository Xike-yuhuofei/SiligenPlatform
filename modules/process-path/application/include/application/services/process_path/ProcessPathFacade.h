#pragma once

#include "domain/trajectory/domain-services/GeometryNormalizer.h"
#include "domain/trajectory/domain-services/ProcessAnnotator.h"
#include "domain/trajectory/domain-services/TrajectoryShaper.h"
#include "process_path/contracts/PathGenerationRequest.h"
#include "process_path/contracts/PathGenerationResult.h"

namespace Siligen::Application::Services::ProcessPath {

using Siligen::Domain::Trajectory::DomainServices::NormalizationConfig;
using Siligen::Domain::Trajectory::DomainServices::NormalizedPath;
using Siligen::Domain::Trajectory::DomainServices::TrajectoryShaperConfig;
using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathGenerationResult;

using ProcessPathBuildRequest = PathGenerationRequest;
using ProcessPathBuildResult = PathGenerationResult;

class ProcessPathFacade {
   public:
    ProcessPathBuildResult Build(const ProcessPathBuildRequest& request) const;
};

}  // namespace Siligen::Application::Services::ProcessPath
