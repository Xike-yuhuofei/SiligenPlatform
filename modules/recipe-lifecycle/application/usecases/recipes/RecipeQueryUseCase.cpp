#include "recipe_lifecycle/application/usecases/recipes/RecipeQueryUseCase.h"

#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Domain::Recipes::ValueObjects::RecipeListFilter;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

RecipeQueryUseCase::RecipeQueryUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port)
    : recipe_repository_(std::move(recipe_repository)),
      template_repository_(std::move(template_repository)),
      audit_repository_(std::move(audit_repository)),
      schema_port_(std::move(schema_port)) {}

Result<GetRecipeResponse> RecipeQueryUseCase::Execute(const GetRecipeRequest& request) {
    if (!recipe_repository_) {
        return Result<GetRecipeResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "GetRecipeUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<GetRecipeResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "GetRecipeUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<GetRecipeResponse>::Failure(recipe_result.GetError());
    }

    GetRecipeResponse response;
    response.recipe = recipe_result.Value();
    return Result<GetRecipeResponse>::Success(response);
}

Result<ListRecipesResponse> RecipeQueryUseCase::Execute(const ListRecipesRequest& request) {
    if (!recipe_repository_) {
        return Result<ListRecipesResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "ListRecipesUseCase"));
    }

    RecipeListFilter filter;
    if (!request.status.empty()) {
        if (request.status == "active") {
            filter.status = RecipeStatus::Active;
        } else if (request.status == "archived") {
            filter.status = RecipeStatus::Archived;
        }
    }
    filter.query = request.query;
    filter.tag = request.tag;

    auto list_result = recipe_repository_->ListRecipes(filter);
    if (list_result.IsError()) {
        return Result<ListRecipesResponse>::Failure(list_result.GetError());
    }

    ListRecipesResponse response;
    response.recipes = list_result.Value();
    return Result<ListRecipesResponse>::Success(response);
}

Result<ListRecipeVersionsResponse> RecipeQueryUseCase::Execute(const ListRecipeVersionsRequest& request) {
    if (!recipe_repository_) {
        return Result<ListRecipeVersionsResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "ListRecipeVersionsUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<ListRecipeVersionsResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "ListRecipeVersionsUseCase"));
    }

    auto versions_result = recipe_repository_->ListVersions(request.recipe_id);
    if (versions_result.IsError()) {
        return Result<ListRecipeVersionsResponse>::Failure(versions_result.GetError());
    }

    ListRecipeVersionsResponse response;
    response.versions = versions_result.Value();
    return Result<ListRecipeVersionsResponse>::Success(response);
}

Result<ListTemplatesResponse> RecipeQueryUseCase::Execute(const ListTemplatesRequest& /*request*/) {
    if (!template_repository_) {
        return Result<ListTemplatesResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Template repository not available", "ListTemplatesUseCase"));
    }

    auto list_result = template_repository_->ListTemplates();
    if (list_result.IsError()) {
        return Result<ListTemplatesResponse>::Failure(list_result.GetError());
    }

    ListTemplatesResponse response;
    response.templates = list_result.Value();
    return Result<ListTemplatesResponse>::Success(response);
}

Result<GetDefaultParameterSchemaResponse> RecipeQueryUseCase::Execute(
    const GetDefaultParameterSchemaRequest& /*request*/) {
    if (!schema_port_) {
        return Result<GetDefaultParameterSchemaResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Parameter schema port not available",
                  "GetDefaultParameterSchemaUseCase"));
    }

    auto schema_result = schema_port_->GetDefaultSchema();
    if (schema_result.IsError()) {
        return Result<GetDefaultParameterSchemaResponse>::Failure(schema_result.GetError());
    }

    GetDefaultParameterSchemaResponse response;
    response.schema = schema_result.Value();
    return Result<GetDefaultParameterSchemaResponse>::Success(response);
}

Result<GetRecipeAuditResponse> RecipeQueryUseCase::Execute(const GetRecipeAuditRequest& request) {
    if (!audit_repository_) {
        return Result<GetRecipeAuditResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Audit repository not available", "GetRecipeAuditUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<GetRecipeAuditResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "GetRecipeAuditUseCase"));
    }

    Result<std::vector<Siligen::Domain::Recipes::ValueObjects::AuditRecord>> list_result(
        Error(ErrorCode::RECIPE_NOT_FOUND, "Audit not found", "GetRecipeAuditUseCase"));
    if (!request.version_id.empty()) {
        list_result = audit_repository_->ListByVersion(request.recipe_id, request.version_id);
    } else {
        list_result = audit_repository_->ListByRecipe(request.recipe_id);
    }
    if (list_result.IsError()) {
        return Result<GetRecipeAuditResponse>::Failure(list_result.GetError());
    }

    GetRecipeAuditResponse response;
    response.records = list_result.Value();
    return Result<GetRecipeAuditResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
