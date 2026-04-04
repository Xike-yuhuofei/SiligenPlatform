#pragma once

#include "process_path/contracts/NormalizedPath.h"
#include "process_path/contracts/PathTopologyDiagnostics.h"
#include "process_path/contracts/ProcessPath.h"

#include <string>

namespace Siligen::ProcessPath::Contracts {

enum class PathGenerationStatus {
    InvalidInput,
    StageFailure,
    Success
};

enum class PathGenerationStage {
    None,
    InputValidation,
    TopologyRepair,
    Normalization,
    ProcessAnnotation,
    TrajectoryShaping
};

struct PathGenerationResult {
    PathGenerationStatus status = PathGenerationStatus::InvalidInput;
    PathGenerationStage failed_stage = PathGenerationStage::InputValidation;
    std::string error_message = "path generation has not produced a valid result";
    NormalizedPath normalized;
    ProcessPath process_path;
    ProcessPath shaped_path;
    PathTopologyDiagnostics topology_diagnostics;
};

}  // namespace Siligen::ProcessPath::Contracts
