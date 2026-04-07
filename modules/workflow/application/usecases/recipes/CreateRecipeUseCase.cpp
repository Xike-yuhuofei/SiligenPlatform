#include "CreateRecipeUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "domain/recipes/domain-services/RecipeValidationService.h"
#include "shared/types/Error.h"

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;

CreateRecipeUseCase::CreateRecipeUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository)
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)) {}

Result<CreateRecipeResponse> CreateRecipeUseCase::Execute(const CreateRecipeRequest& request) {
    if (!recipe_repository_) {
        return Result<CreateRecipeResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "CreateRecipeUseCase"));
    }
    if (request.name.empty()) {
        return Result<CreateRecipeResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe name is required", "CreateRecipeUseCase"));
    }

    auto existing = recipe_repository_->GetRecipeByName(request.name);
    if (existing.IsSuccess()) {
        return Result<CreateRecipeResponse>::Failure(
            Error(ErrorCode::RECIPE_ALREADY_EXISTS, "Recipe name already exists", "CreateRecipeUseCase"));
    }

    Siligen::Domain::Recipes::Aggregates::Recipe recipe;
    recipe.id = GenerateId("recipe");
    recipe.name = request.name;
    recipe.description = request.description;
    recipe.tags = request.tags;
    recipe.status = RecipeStatus::Active;
    recipe.created_at = NowEpochMillis();
    recipe.updated_at = recipe.created_at;

    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipe(recipe);
    if (validation.IsError()) {
        return Result<CreateRecipeResponse>::Failure(validation.GetError());
    }

    auto save_result = recipe_repository_->SaveRecipe(recipe);
    if (save_result.IsError()) {
        return Result<CreateRecipeResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = recipe.id;
        record.action = AuditAction::Create;
        record.actor = request.actor.empty() ? "system" : request.actor;
        record.timestamp = NowEpochMillis();
        record.changes = DiffStrings("name", "", recipe.name);
        auto desc_changes = DiffStrings("description", "", recipe.description);
        record.changes.insert(record.changes.end(), desc_changes.begin(), desc_changes.end());
        auto tag_changes = DiffTags({}, recipe.tags);
        record.changes.insert(record.changes.end(), tag_changes.begin(), tag_changes.end());
        audit_repository_->AppendRecord(record);
    }

    CreateRecipeResponse response;
    response.recipe = recipe;
    return Result<CreateRecipeResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes

