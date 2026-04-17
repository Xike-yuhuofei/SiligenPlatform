#include "recipe_lifecycle/application/usecases/recipes/CreateVersionFromPublishedUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "recipe_lifecycle/domain/recipes/domain-services/RecipeValidationService.h"
#include "shared/types/Error.h"

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;

CreateVersionFromPublishedUseCase::CreateVersionFromPublishedUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository)
    : recipe_repository_(std::move(recipe_repository)),
      schema_port_(std::move(schema_port)),
      audit_repository_(std::move(audit_repository)) {}

Result<CreateVersionFromPublishedResponse> CreateVersionFromPublishedUseCase::Execute(
    const CreateVersionFromPublishedRequest& request) {
    if (!recipe_repository_ || !schema_port_) {
        return Result<CreateVersionFromPublishedResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe dependencies not available", "CreateVersionFromPublishedUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "CreateVersionFromPublishedUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(recipe_result.GetError());
    }

    std::string base_version_id = request.base_version_id;
    if (base_version_id.empty()) {
        base_version_id = recipe_result.Value().active_version_id;
    }
    if (base_version_id.empty()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Base version id is required", "CreateVersionFromPublishedUseCase"));
    }

    auto base_result = recipe_repository_->GetVersionById(request.recipe_id, base_version_id);
    if (base_result.IsError()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(base_result.GetError());
    }
    auto base_version = base_result.Value();
    if (base_version.status != RecipeVersionStatus::Published) {
        return Result<CreateVersionFromPublishedResponse>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Base version must be published", "CreateVersionFromPublishedUseCase"));
    }

    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
    version.id = GenerateId("version");
    version.recipe_id = request.recipe_id;
    version.version_label = request.version_label.empty() ? "draft-" + std::to_string(NowEpochMillis())
                                                          : request.version_label;
    version.status = RecipeVersionStatus::Draft;
    version.base_version_id = base_version_id;
    version.parameters = base_version.parameters;
    version.created_by = request.created_by;
    version.created_at = NowEpochMillis();
    version.change_note = request.change_note;
    NormalizeRecipePlanningPolicyParameters(version.parameters);

    auto schema_result = schema_port_->GetDefaultSchema();
    if (schema_result.IsError()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(schema_result.GetError());
    }
    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipeVersion(version, schema_result.Value());
    if (validation.IsError()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(validation.GetError());
    }

    auto save_result = recipe_repository_->SaveVersion(version);
    if (save_result.IsError()) {
        return Result<CreateVersionFromPublishedResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = request.recipe_id;
        record.version_id = version.id;
        record.action = AuditAction::Edit;
        record.actor = request.created_by.empty() ? "system" : request.created_by;
        record.timestamp = NowEpochMillis();
        record.changes = DiffStrings("baseVersionId", "", base_version_id);
        audit_repository_->AppendRecord(record);
    }

    CreateVersionFromPublishedResponse response;
    response.version = version;
    return Result<CreateVersionFromPublishedResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
