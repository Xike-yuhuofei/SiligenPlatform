#include "recipe_lifecycle/domain/recipes/domain-services/RecipeActivationService.h"

#include "shared/types/Error.h"

namespace Siligen::Domain::Recipes::DomainServices {
namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using ValueObjects::AuditRecord;

constexpr const char* kModuleName = "RecipeActivationService";

std::vector<ValueObjects::FieldChange> DiffStrings(
    const std::string& field,
    const std::string& before,
    const std::string& after) {
    if (before == after) {
        return {};
    }

    ValueObjects::FieldChange change;
    change.field = field;
    change.before = before;
    change.after = after;
    return {change};
}

}  // namespace

RecipeActivationService::RecipeActivationService(
    std::shared_ptr<Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Ports::IAuditRepositoryPort> audit_repository) noexcept
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)) {}

Result<ActivateRecipeVersionResult> RecipeActivationService::ActivateVersion(
    const RecipeId& recipe_id,
    const RecipeVersionId& version_id,
    AuditAction audit_action,
    const std::string& actor,
    std::int64_t timestamp,
    const AuditRecordId& audit_id) noexcept {
    if (!recipe_repository_) {
        return Result<ActivateRecipeVersionResult>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", kModuleName));
    }
    if (recipe_id.empty() || version_id.empty()) {
        return Result<ActivateRecipeVersionResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version id are required", kModuleName));
    }

    auto recipe_result = recipe_repository_->GetRecipeById(recipe_id);
    if (recipe_result.IsError()) {
        return Result<ActivateRecipeVersionResult>::Failure(recipe_result.GetError());
    }

    auto recipe = recipe_result.Value();
    if (recipe.status == ValueObjects::RecipeStatus::Archived) {
        return Result<ActivateRecipeVersionResult>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Recipe is archived", kModuleName));
    }

    auto version_result = recipe_repository_->GetVersionById(recipe_id, version_id);
    if (version_result.IsError()) {
        return Result<ActivateRecipeVersionResult>::Failure(version_result.GetError());
    }
    if (version_result.Value().status != ValueObjects::RecipeVersionStatus::Published) {
        return Result<ActivateRecipeVersionResult>::Failure(
            Error(ErrorCode::RECIPE_INVALID_STATE, "Only published versions can be activated", kModuleName));
    }

    recipe.active_version_id = version_id;
    recipe.updated_at = timestamp;

    auto save_result = recipe_repository_->SaveRecipe(recipe);
    if (save_result.IsError()) {
        return Result<ActivateRecipeVersionResult>::Failure(save_result.GetError());
    }

    if (audit_repository_) {
        AuditRecord record;
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
    return Result<ActivateRecipeVersionResult>::Success(result);
}

}  // namespace Siligen::Domain::Recipes::DomainServices
