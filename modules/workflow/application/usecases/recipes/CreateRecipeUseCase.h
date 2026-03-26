#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct CreateRecipeRequest {
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::string actor;
};

struct CreateRecipeResponse {
    Siligen::Domain::Recipes::Aggregates::Recipe recipe;
};

class CreateRecipeUseCase {
   public:
    CreateRecipeUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository);

    Siligen::Shared::Types::Result<CreateRecipeResponse> Execute(const CreateRecipeRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
