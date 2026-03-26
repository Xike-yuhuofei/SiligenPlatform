#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct UpdateRecipeRequest {
    std::string recipe_id;
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    bool update_name = false;
    bool update_description = false;
    bool update_tags = false;
    std::string actor;
};

struct UpdateRecipeResponse {
    Siligen::Domain::Recipes::Aggregates::Recipe recipe;
};

class UpdateRecipeUseCase {
   public:
    UpdateRecipeUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository);

    Siligen::Shared::Types::Result<UpdateRecipeResponse> Execute(const UpdateRecipeRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
