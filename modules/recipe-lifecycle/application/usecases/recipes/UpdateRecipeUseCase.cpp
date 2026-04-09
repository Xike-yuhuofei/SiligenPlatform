#include "recipe_lifecycle/application/usecases/recipes/UpdateRecipeUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "recipe_lifecycle/domain/recipes/domain-services/RecipeValidationService.h"
#include "shared/types/Error.h"

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;

UpdateRecipeUseCase::UpdateRecipeUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository)
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)) {}

Result<UpdateRecipeResponse> UpdateRecipeUseCase::Execute(const UpdateRecipeRequest& request) {
    if (!recipe_repository_) {
        return Result<UpdateRecipeResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "UpdateRecipeUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<UpdateRecipeResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "UpdateRecipeUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<UpdateRecipeResponse>::Failure(recipe_result.GetError());
    }

    auto recipe = recipe_result.Value();
    if (recipe.status == RecipeStatus::Archived) {
        return Result<UpdateRecipeResponse>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Recipe is archived", "UpdateRecipeUseCase"));
    }

    std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> changes;
    if (request.update_name && !request.name.empty() && request.name != recipe.name) {
        auto existing = recipe_repository_->GetRecipeByName(request.name);
        if (existing.IsSuccess() && existing.Value().id != recipe.id) {
            return Result<UpdateRecipeResponse>::Failure(
                Error(ErrorCode::RECIPE_ALREADY_EXISTS, "Recipe name already exists", "UpdateRecipeUseCase"));
        }
        auto diff = DiffStrings("name", recipe.name, request.name);
        changes.insert(changes.end(), diff.begin(), diff.end());
        recipe.name = request.name;
    }

    if (request.update_description && request.description != recipe.description) {
        auto diff = DiffStrings("description", recipe.description, request.description);
        changes.insert(changes.end(), diff.begin(), diff.end());
        recipe.description = request.description;
    }

    if (request.update_tags) {
        auto diff = DiffTags(recipe.tags, request.tags);
        changes.insert(changes.end(), diff.begin(), diff.end());
        recipe.tags = request.tags;
    }

    recipe.updated_at = NowEpochMillis();

    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipe(recipe);
    if (validation.IsError()) {
        return Result<UpdateRecipeResponse>::Failure(validation.GetError());
    }

    auto save_result = recipe_repository_->SaveRecipe(recipe);
    if (save_result.IsError()) {
        return Result<UpdateRecipeResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_ && !changes.empty()) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = recipe.id;
        record.action = AuditAction::Edit;
        record.actor = request.actor.empty() ? "system" : request.actor;
        record.timestamp = NowEpochMillis();
        record.changes = changes;
        audit_repository_->AppendRecord(record);
    }

    UpdateRecipeResponse response;
    response.recipe = recipe;
    return Result<UpdateRecipeResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
