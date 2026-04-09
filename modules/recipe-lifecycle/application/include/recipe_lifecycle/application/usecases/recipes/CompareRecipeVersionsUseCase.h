#pragma once

#include "recipe_lifecycle/domain/recipes/ports/IRecipeRepositoryPort.h"
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct CompareRecipeVersionsRequest {
    std::string recipe_id;
    std::string from_version_id;
    std::string to_version_id;
};

struct CompareRecipeVersionsResponse {
    std::string recipe_id;
    std::string from_version_id;
    std::string to_version_id;
    std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> changes;
};

class CompareRecipeVersionsUseCase {
   public:
    explicit CompareRecipeVersionsUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository);

    Siligen::Shared::Types::Result<CompareRecipeVersionsResponse> Execute(
        const CompareRecipeVersionsRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
