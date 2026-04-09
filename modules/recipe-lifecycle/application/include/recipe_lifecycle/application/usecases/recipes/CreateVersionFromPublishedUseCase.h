#pragma once

#include "recipe_lifecycle/domain/recipes/aggregates/RecipeVersion.h"
#include "recipe_lifecycle/domain/recipes/ports/IAuditRepositoryPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IParameterSchemaPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Recipes {

struct CreateVersionFromPublishedRequest {
    std::string recipe_id;
    std::string base_version_id;
    std::string version_label;
    std::string created_by;
    std::string change_note;
};

struct CreateVersionFromPublishedResponse {
    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
};

class CreateVersionFromPublishedUseCase {
   public:
    CreateVersionFromPublishedUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository);

    Siligen::Shared::Types::Result<CreateVersionFromPublishedResponse> Execute(
        const CreateVersionFromPublishedRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
