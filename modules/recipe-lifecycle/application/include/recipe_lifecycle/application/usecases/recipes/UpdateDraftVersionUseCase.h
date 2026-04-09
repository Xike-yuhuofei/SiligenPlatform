#pragma once

#include "recipe_lifecycle/domain/recipes/aggregates/RecipeVersion.h"
#include "recipe_lifecycle/domain/recipes/ports/IAuditRepositoryPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IParameterSchemaPort.h"
#include "recipe_lifecycle/domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct UpdateDraftVersionRequest {
    std::string recipe_id;
    std::string version_id;
    std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry> parameters;
    std::string change_note;
    std::string editor;
};

struct UpdateDraftVersionResponse {
    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
};

class UpdateDraftVersionUseCase {
   public:
    UpdateDraftVersionUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository);

    Siligen::Shared::Types::Result<UpdateDraftVersionResponse> Execute(const UpdateDraftVersionRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
