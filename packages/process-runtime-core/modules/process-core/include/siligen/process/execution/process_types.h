#pragma once

#include <cstddef>
#include <string>

namespace Siligen::Process::Execution {

enum class ProcessRunMode {
    kDryRun,
    kProduction,
    kPreview,
    kPurge,
};

struct RecipeReference {
    std::string recipe_id;
    std::string version_id;
};

struct ProcessExecutionRequest {
    RecipeReference recipe;
    std::string source_artifact;
    ProcessRunMode mode = ProcessRunMode::kProduction;
    bool allow_partial_execution = false;
};

struct ProcessExecutionSummary {
    std::string execution_id;
    std::size_t planned_steps = 0;
    bool motion_required = true;
    bool dispensing_required = true;
};

}  // namespace Siligen::Process::Execution
