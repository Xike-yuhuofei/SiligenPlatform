#include "recipe_lifecycle/application/usecases/recipes/UpdateDraftVersionUseCase.h"

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

UpdateDraftVersionUseCase::UpdateDraftVersionUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository)
    : recipe_repository_(std::move(recipe_repository)),
      schema_port_(std::move(schema_port)),
      audit_repository_(std::move(audit_repository)) {}

Result<UpdateDraftVersionResponse> UpdateDraftVersionUseCase::Execute(const UpdateDraftVersionRequest& request) {
    if (!recipe_repository_ || !schema_port_) {
        return Result<UpdateDraftVersionResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe dependencies not available", "UpdateDraftVersionUseCase"));
    }
    if (request.recipe_id.empty() || request.version_id.empty()) {
        return Result<UpdateDraftVersionResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version id are required", "UpdateDraftVersionUseCase"));
    }

    auto version_result = recipe_repository_->GetVersionById(request.recipe_id, request.version_id);
    if (version_result.IsError()) {
        return Result<UpdateDraftVersionResponse>::Failure(version_result.GetError());
    }

    auto version = version_result.Value();
    if (version.status != RecipeVersionStatus::Draft) {
        return Result<UpdateDraftVersionResponse>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Only draft versions can be edited", "UpdateDraftVersionUseCase"));
    }

    auto before_params = version.parameters;
    version.parameters = request.parameters;
    if (!request.change_note.empty()) {
        version.change_note = request.change_note;
    }
    NormalizeRecipePlanningPolicyParameters(version.parameters);

    auto schema_result = schema_port_->GetDefaultSchema();
    if (schema_result.IsError()) {
        return Result<UpdateDraftVersionResponse>::Failure(schema_result.GetError());
    }
    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipeVersion(version, schema_result.Value());
    if (validation.IsError()) {
        return Result<UpdateDraftVersionResponse>::Failure(validation.GetError());
    }

    auto save_result = recipe_repository_->SaveVersion(version);
    if (save_result.IsError()) {
        return Result<UpdateDraftVersionResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = request.recipe_id;
        record.version_id = request.version_id;
        record.action = AuditAction::Edit;
        record.actor = request.editor.empty() ? "system" : request.editor;
        record.timestamp = NowEpochMillis();
        record.changes = DiffParameters(before_params, version.parameters);
        audit_repository_->AppendRecord(record);
    }

    UpdateDraftVersionResponse response;
    response.version = version;
    return Result<UpdateDraftVersionResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
