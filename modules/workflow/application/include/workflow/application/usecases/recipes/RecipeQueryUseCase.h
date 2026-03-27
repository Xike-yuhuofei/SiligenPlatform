#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IParameterSchemaPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/ports/ITemplateRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct GetRecipeRequest {
    std::string recipe_id;
};

struct GetRecipeResponse {
    Siligen::Domain::Recipes::Aggregates::Recipe recipe;
};

struct ListRecipesRequest {
    std::string status;
    std::string query;
    std::string tag;
};

struct ListRecipesResponse {
    std::vector<Siligen::Domain::Recipes::Aggregates::Recipe> recipes;
};

struct ListRecipeVersionsRequest {
    std::string recipe_id;
};

struct ListRecipeVersionsResponse {
    std::vector<Siligen::Domain::Recipes::Aggregates::RecipeVersion> versions;
};

struct ListTemplatesRequest {
};

struct ListTemplatesResponse {
    std::vector<Siligen::Domain::Recipes::ValueObjects::Template> templates;
};

struct GetDefaultParameterSchemaRequest {
};

struct GetDefaultParameterSchemaResponse {
    Siligen::Domain::Recipes::ValueObjects::ParameterSchema schema;
};

struct GetRecipeAuditRequest {
    std::string recipe_id;
    std::string version_id;
};

struct GetRecipeAuditResponse {
    std::vector<Siligen::Domain::Recipes::ValueObjects::AuditRecord> records;
};

class RecipeQueryUseCase {
   public:
    RecipeQueryUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port);

    Siligen::Shared::Types::Result<GetRecipeResponse> Execute(const GetRecipeRequest& request);
    Siligen::Shared::Types::Result<ListRecipesResponse> Execute(const ListRecipesRequest& request);
    Siligen::Shared::Types::Result<ListRecipeVersionsResponse> Execute(const ListRecipeVersionsRequest& request);
    Siligen::Shared::Types::Result<ListTemplatesResponse> Execute(const ListTemplatesRequest& request);
    Siligen::Shared::Types::Result<GetDefaultParameterSchemaResponse> Execute(
        const GetDefaultParameterSchemaRequest& request);
    Siligen::Shared::Types::Result<GetRecipeAuditResponse> Execute(const GetRecipeAuditRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port_;
};

}  // namespace Siligen::Application::UseCases::Recipes
