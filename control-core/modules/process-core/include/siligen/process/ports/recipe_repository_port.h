#pragma once

#include "siligen/process/execution/process_types.h"
#include "siligen/shared/result_types.h"

#include <string>

namespace Siligen::Process::Ports {

class RecipeRepositoryPort {
   public:
    virtual ~RecipeRepositoryPort() = default;

    virtual Siligen::SharedKernel::Result<Siligen::Process::Execution::RecipeReference> GetActiveRecipe(
        const std::string& recipe_id) const = 0;
    virtual Siligen::SharedKernel::Result<Siligen::Process::Execution::RecipeReference> SaveDraftRecipe(
        const Siligen::Process::Execution::RecipeReference& recipe) = 0;
};

}  // namespace Siligen::Process::Ports
