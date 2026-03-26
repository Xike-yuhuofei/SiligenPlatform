#include "siligen/process/recipes/services/recipe_activation_service.h"

#include "siligen/shared/error.h"

namespace Siligen::Process::Recipes::Services {
namespace {

constexpr const char* kModuleName = "RecipeActivationService";

}  // namespace

RecipeActivationService::RecipeActivationService(
    std::shared_ptr<Ports::RecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Ports::AuditRepositoryPort> audit_repository) noexcept
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)) {}

Siligen::SharedKernel::Result<ActivateRecipeVersionResult> RecipeActivationService::ActivateVersion(
    const ValueObjects::RecipeId& recipe_id,
    const ValueObjects::RecipeVersionId& version_id,
    ValueObjects::AuditAction audit_action,
    const std::string& actor,
    std::int64_t timestamp,
    const ValueObjects::AuditRecordId& audit_id) noexcept {
    if (!recipe_repository_) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(
            Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                         "Recipe repository not available",
                                         kModuleName));
    }
    if (recipe_id.empty() || version_id.empty()) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(
            Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::INVALID_PARAMETER,
                                         "Recipe id and version id are required",
                                         kModuleName));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(recipe_id);
    if (recipe_result.IsError()) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(recipe_result.GetError());
    }

    auto recipe = recipe_result.Value();
    if (recipe.status == ValueObjects::RecipeStatus::Archived) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(
            Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::RECIPE_INVALID_STATE,
                                         "Recipe is archived",
                                         kModuleName));
    }

    auto version_result = recipe_repository_->GetVersionById(recipe_id, version_id);
    if (version_result.IsError()) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(version_result.GetError());
    }
    if (version_result.Value().status != ValueObjects::RecipeVersionStatus::Published) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(
            Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::RECIPE_INVALID_STATE,
                                         "Only published versions can be activated",
                                         kModuleName));
    }

    recipe.active_version_id = version_id;
    recipe.updated_at = timestamp;

    auto save_result = recipe_repository_->SaveRecipe(recipe);
    if (save_result.IsError()) {
        return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        ValueObjects::AuditRecord record;
        record.id = audit_id;
        record.recipe_id = recipe_id;
        record.version_id = version_id;
        record.action = audit_action;
        record.actor = actor.empty() ? "system" : actor;
        record.timestamp = timestamp;
        record.changes = DiffStrings("activeVersionId", "", version_id);
        static_cast<void>(audit_repository_->AppendRecord(record));
    }

    ActivateRecipeVersionResult result;
    result.recipe_id = recipe_id;
    result.active_version_id = version_id;
    return Siligen::SharedKernel::Result<ActivateRecipeVersionResult>::Success(result);
}

std::vector<ValueObjects::FieldChange> RecipeActivationService::DiffStrings(
    const std::string& field,
    const std::string& before,
    const std::string& after) noexcept {
    if (before == after) {
        return {};
    }
    ValueObjects::FieldChange change;
    change.field = field;
    change.before = before;
    change.after = after;
    return {change};
}

}  // namespace Siligen::Process::Recipes::Services
