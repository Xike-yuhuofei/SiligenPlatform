#pragma once

#include "process_path/contracts/PathGenerationRequest.h"
#include "process_path/contracts/PathGenerationResult.h"

namespace Siligen::Application::Services::ProcessPath {

using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathGenerationResult;

using ProcessPathBuildRequest = PathGenerationRequest;
using ProcessPathBuildResult = PathGenerationResult;

class ProcessPathFacade {
   public:
    ProcessPathBuildResult Build(const ProcessPathBuildRequest& request) const;
};

}  // namespace Siligen::Application::Services::ProcessPath
