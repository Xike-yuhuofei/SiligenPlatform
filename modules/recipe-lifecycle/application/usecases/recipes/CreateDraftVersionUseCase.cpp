#include "recipe_lifecycle/application/usecases/recipes/CreateDraftVersionUseCase.h"

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
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;

CreateDraftVersionUseCase::CreateDraftVersionUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IParameterSchemaPort> schema_port,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository)
    : recipe_repository_(std::move(recipe_repository)),
      template_repository_(std::move(template_repository)),
      schema_port_(std::move(schema_port)),
      audit_repository_(std::move(audit_repository)) {}

Result<CreateDraftVersionResponse> CreateDraftVersionUseCase::Execute(const CreateDraftVersionRequest& request) {
    if (!recipe_repository_ || !schema_port_) {
        return Result<CreateDraftVersionResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe dependencies not available", "CreateDraftVersionUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<CreateDraftVersionResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "CreateDraftVersionUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<CreateDraftVersionResponse>::Failure(recipe_result.GetError());
    }
    if (recipe_result.Value().status == RecipeStatus::Archived) {
        return Result<CreateDraftVersionResponse>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Recipe is archived", "CreateDraftVersionUseCase"));
    }

    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
    version.id = GenerateId("version");
    version.recipe_id = request.recipe_id;
    version.version_label = request.version_label.empty() ? "draft-" + std::to_string(NowEpochMillis())
                                                          : request.version_label;
    version.status = RecipeVersionStatus::Draft;
    version.base_version_id = request.base_version_id;
    version.created_by = request.created_by;
    version.created_at = NowEpochMillis();
    version.change_note = request.change_note;

    if (!request.template_id.empty()) {
        if (!template_repository_) {
            return Result<CreateDraftVersionResponse>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "Template repository not available", "CreateDraftVersionUseCase"));
        }
        auto template_result = template_repository_->GetTemplateById(request.template_id);
        if (template_result.IsError()) {
            return Result<CreateDraftVersionResponse>::Failure(template_result.GetError());
        }
        version.parameters = template_result.Value().parameters;
    } else if (!request.base_version_id.empty()) {
        auto base_result = recipe_repository_->GetVersionById(request.recipe_id, request.base_version_id);
        if (base_result.IsError()) {
            return Result<CreateDraftVersionResponse>::Failure(base_result.GetError());
        }
        version.parameters = base_result.Value().parameters;
    }
    NormalizeRecipePlanningPolicyParameters(version.parameters);

    auto schema_result = schema_port_->GetDefaultSchema();
    if (schema_result.IsError()) {
        return Result<CreateDraftVersionResponse>::Failure(schema_result.GetError());
    }
    Siligen::Domain::Recipes::DomainServices::RecipeValidationService validation_service;
    auto validation = validation_service.ValidateRecipeVersion(version, schema_result.Value());
    if (validation.IsError()) {
        return Result<CreateDraftVersionResponse>::Failure(validation.GetError());
    }

    auto save_result = recipe_repository_->SaveVersion(version);
    if (save_result.IsError()) {
        return Result<CreateDraftVersionResponse>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
        record.id = GenerateId("audit");
        record.recipe_id = request.recipe_id;
        record.version_id = version.id;
        record.action = AuditAction::Edit;
        record.actor = request.created_by.empty() ? "system" : request.created_by;
        record.timestamp = NowEpochMillis();
        record.changes = DiffStrings("versionLabel", "", version.version_label);
        audit_repository_->AppendRecord(record);
    }

    CreateDraftVersionResponse response;
    response.version = version;
    return Result<CreateDraftVersionResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
