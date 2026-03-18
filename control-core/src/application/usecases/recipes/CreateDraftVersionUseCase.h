#pragma once

#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IParameterSchemaPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/ports/ITemplateRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Recipes {

struct CreateDraftVersionRequest {
    std::string recipe_id;
    std::string template_id;
    std::string base_version_id;
    std::string version_label;
    std::string created_by;
    std::string change_note;
};

struct CreateDraftVersionResponse {
    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
};

class CreateDraftVersionUseCase {
   public:
    CreateDraftVersionUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository);

    Siligen::Shared::Types::Result<CreateDraftVersionResponse> Execute(const CreateDraftVersionRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Application::UseCases::Recipes
