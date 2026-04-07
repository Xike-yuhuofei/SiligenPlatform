#include "RecipeCommandUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "domain/recipes/domain-services/RecipeActivationService.h"
#include "domain/recipes/domain-services/RecipeValidationService.h"
#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

RecipeCommandUseCase::RecipeCommandUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port)
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)),
      schema_port_(std::move(schema_port)) {}

Result<ActivateRecipeVersionResponse> RecipeCommandUseCase::Execute(const ActivateRecipeVersionRequest& request) {
    if (!recipe_repository_) {
        return Result<ActivateRecipeVersionResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "ActivateRecipeVersionUseCase"));
    }

    Siligen::Domain::Recipes::DomainServices::RecipeActivationService activation_service(
        recipe_repository_,
        audit_repository_);
    auto activation_result = activation_service.ActivateVersion(
        request.recipe_id,
        request.version_id,
        AuditAction::Rollback,
        request.actor,
        NowEpochMillis(),
        GenerateId("audit"));
    if (activation_result.IsError()) {
        return Result<ActivateRecipeVersionResponse>::Failure(activation_result.GetError());
    }

    ActivateRecipeVersionResponse response;
    response.active_version_id = activation_result.Value().active_version_id;
    return Result<ActivateRecipeVersionResponse>::Success(response);
}

Result<ArchiveRecipeResponse> RecipeCommandUseCase::Execute(const ArchiveRecipeRequest& request) {
    if (!recipe_repository_) {
        return Result<ArchiveRecipeResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "ArchiveRecipeUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<ArchiveRecipeResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "ArchiveRecipeUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<ArchiveRecipeResponse>::Failure(recipe_result.GetError());
    }
    auto recipe = recipe_result.Value();
    if (recipe.status == RecipeStatus::Archived) {
        ArchiveRecipeResponse response;
        response.archived = true;
        return Result<ArchiveRecipeResponse>::Success(response);
    }

    recipe.status = RecipeStatus::Archived;
    recipe.updated_at = NowEpochMillis();
    auto save_result = recipe_repository_->SaveRecipe(recipe);
    if (save_result.IsError()) {
        return Result<ArchiveRecipeResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = request.recipe_id;
        record.action = AuditAction::Archive;
        record.actor = request.actor.empty() ? "system" : request.actor;
        record.timestamp = NowEpochMillis();
        record.changes = DiffStrings("status", "active", "archived");
        audit_repository_->AppendRecord(record);
    }

    ArchiveRecipeResponse response;
    response.archived = true;
    return Result<ArchiveRecipeResponse>::Success(response);
}

Result<PublishRecipeVersionResponse> RecipeCommandUseCase::Execute(
    const PublishRecipeVersionRequest& request) {
    if (!recipe_repository_ || !schema_port_) {
        return Result<PublishRecipeVersionResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe dependencies not available", "PublishRecipeVersionUseCase"));
    }
    if (request.recipe_id.empty() || request.version_id.empty()) {
        return Result<PublishRecipeVersionResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version id are required", "PublishRecipeVersionUseCase"));
    }

    auto version_result = recipe_repository_->GetVersionById(request.recipe_id, request.version_id);
    if (version_result.IsError()) {
        return Result<PublishRecipeVersionResponse>::Failure(version_result.GetError());
    }

    auto version = version_result.Value();
    if (version.status != RecipeVersionStatus::Draft) {
        return Result<PublishRecipeVersionResponse>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Only draft versions can be published", "PublishRecipeVersionUseCase"));
    }

    auto schema_result = schema_port_->GetDefaultSchema();
    if (schema_result.IsError()) {
        return Result<PublishRecipeVersionResponse>::Failure(schema_result.GetError());
    }
    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipeVersion(version, schema_result.Value());
    if (validation.IsError()) {
        return Result<PublishRecipeVersionResponse>::Failure(validation.GetError());
    }

    version.status = RecipeVersionStatus::Published;
    version.published_at = NowEpochMillis();

    auto save_version_result = recipe_repository_->SaveVersion(version);
    if (save_version_result.IsError()) {
        return Result<PublishRecipeVersionResponse>::Failure(save_version_result.GetError());
    }

    Siligen::Domain::Recipes::DomainServices::RecipeActivationService activation_service(
        recipe_repository_,
        audit_repository_);
    auto activation_result = activation_service.ActivateVersion(
        request.recipe_id,
        request.version_id,
        AuditAction::Publish,
        request.actor,
        NowEpochMillis(),
        GenerateId("audit"));
    if (activation_result.IsError()) {
        return Result<PublishRecipeVersionResponse>::Failure(activation_result.GetError());
    }

    PublishRecipeVersionResponse response;
    response.version = version;
    return Result<PublishRecipeVersionResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
