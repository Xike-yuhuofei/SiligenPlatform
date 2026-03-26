#pragma once

#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IParameterSchemaPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Recipes {

struct ActivateRecipeVersionRequest {
    std::string recipe_id;
    std::string version_id;
    std::string actor;
};

struct ActivateRecipeVersionResponse {
    std::string active_version_id;
};

struct ArchiveRecipeRequest {
    std::string recipe_id;
    std::string actor;
};

struct ArchiveRecipeResponse {
    bool archived = false;
};

struct PublishRecipeVersionRequest {
    std::string recipe_id;
    std::string version_id;
    std::string actor;
};

struct PublishRecipeVersionResponse {
    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
};

class RecipeCommandUseCase {
   public:
    RecipeCommandUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port);

    Siligen::Shared::Types::Result<ActivateRecipeVersionResponse> Execute(
        const ActivateRecipeVersionRequest& request);
    Siligen::Shared::Types::Result<ArchiveRecipeResponse> Execute(const ArchiveRecipeRequest& request);
    Siligen::Shared::Types::Result<PublishRecipeVersionResponse> Execute(
        const PublishRecipeVersionRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port_;
};

}  // namespace Siligen::Application::UseCases::Recipes
