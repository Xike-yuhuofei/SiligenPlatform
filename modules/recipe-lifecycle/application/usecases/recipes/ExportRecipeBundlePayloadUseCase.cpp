#include "recipe_lifecycle/application/usecases/recipes/ExportRecipeBundlePayloadUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeBundle.h"
#include "shared/types/Error.h"

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Domain::Recipes::ValueObjects::RecipeBundle;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

ExportRecipeBundlePayloadUseCase::ExportRecipeBundlePayloadUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port)
    : recipe_repository_(std::move(recipe_repository)),
      template_repository_(std::move(template_repository)),
      audit_repository_(std::move(audit_repository)),
      serializer_port_(std::move(serializer_port)) {}

Result<ExportRecipeBundlePayloadResponse> ExportRecipeBundlePayloadUseCase::Execute(
    const ExportRecipeBundlePayloadRequest& request) {
    if (!recipe_repository_ || !serializer_port_) {
        return Result<ExportRecipeBundlePayloadResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED,
                  "Recipe dependencies not available",
                  "ExportRecipeBundlePayloadUseCase"));
    }
    if (request.recipe_id.empty()) {
        return Result<ExportRecipeBundlePayloadResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", "ExportRecipeBundlePayloadUseCase"));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(request.recipe_id);
    if (recipe_result.IsError()) {
        return Result<ExportRecipeBundlePayloadResponse>::Failure(recipe_result.GetError());
    }

    RecipeBundle bundle;
    bundle.schema_version = "1.0";
    bundle.exported_at = NowEpochMillis();
    bundle.recipes.push_back(recipe_result.Value());

    auto versions_result = recipe_repository_->ListVersions(request.recipe_id);
    if (versions_result.IsError()) {
        return Result<ExportRecipeBundlePayloadResponse>::Failure(versions_result.GetError());
    }
    bundle.versions = versions_result.Value();

    if (template_repository_) {
        auto templates_result = template_repository_->ListTemplates();
        if (templates_result.IsSuccess()) {
            bundle.templates = templates_result.Value();
        }
    }

    if (audit_repository_) {
        auto audit_result = audit_repository_->ListByRecipe(request.recipe_id);
        if (audit_result.IsSuccess()) {
            bundle.audit_records = audit_result.Value();
        }
    }

    auto serialize_result = serializer_port_->Serialize(bundle);
    if (serialize_result.IsError()) {
        return Result<ExportRecipeBundlePayloadResponse>::Failure(serialize_result.GetError());
    }

    ExportRecipeBundlePayloadResponse response;
    response.bundle_json = serialize_result.Value();
    response.recipe_count = bundle.recipes.size();
    response.version_count = bundle.versions.size();
    response.audit_count = bundle.audit_records.size();
    return Result<ExportRecipeBundlePayloadResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
