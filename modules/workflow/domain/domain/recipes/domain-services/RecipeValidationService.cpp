#include "domain/recipes/domain-services/RecipeValidationService.h"

#include "siligen/process/recipes/services/recipe_validation_service.h"

namespace Siligen::Domain::Recipes::DomainServices {
namespace {

Siligen::Process::Recipes::ValueObjects::ParameterSchema ToNewSchema(const ParameterSchema& schema) {
    Siligen::Process::Recipes::ValueObjects::ParameterSchema converted;
    converted.schema_id = schema.schema_id;
    converted.entries.reserve(schema.entries.size());
    for (const auto& entry : schema.entries) {
        Siligen::Process::Recipes::ValueObjects::ParameterSchemaEntry new_entry;
        new_entry.key = entry.key;
        new_entry.display_name = entry.display_name;
        new_entry.value_type = static_cast<Siligen::Process::Recipes::ValueObjects::ParameterValueType>(entry.value_type);
        new_entry.unit = entry.unit;
        new_entry.constraints.min_value = entry.constraints.min_value;
        new_entry.constraints.max_value = entry.constraints.max_value;
        new_entry.constraints.allowed_values = entry.constraints.allowed_values;
        new_entry.required = entry.required;
        converted.entries.push_back(std::move(new_entry));
    }
    return converted;
}

Siligen::Process::Recipes::Aggregates::Recipe ToNewRecipe(const Recipe& recipe) {
    Siligen::Process::Recipes::Aggregates::Recipe converted;
    converted.id = recipe.id;
    converted.name = recipe.name;
    converted.description = recipe.description;
    converted.status = static_cast<Siligen::Process::Recipes::ValueObjects::RecipeStatus>(recipe.status);
    converted.tags = recipe.tags;
    converted.active_version_id = recipe.active_version_id;
    converted.version_ids = recipe.version_ids;
    converted.created_at = recipe.created_at;
    converted.updated_at = recipe.updated_at;
    return converted;
}

Siligen::Process::Recipes::Aggregates::RecipeVersion ToNewRecipeVersion(const RecipeVersion& version) {
    Siligen::Process::Recipes::Aggregates::RecipeVersion converted;
    converted.id = version.id;
    converted.recipe_id = version.recipe_id;
    converted.version_label = version.version_label;
    converted.status = static_cast<Siligen::Process::Recipes::ValueObjects::RecipeVersionStatus>(version.status);
    converted.base_version_id = version.base_version_id;
    converted.created_by = version.created_by;
    converted.created_at = version.created_at;
    converted.published_at = version.published_at;
    converted.change_note = version.change_note;
    converted.parameters.reserve(version.parameters.size());
    for (const auto& param : version.parameters) {
        converted.parameters.push_back({param.key, param.value});
    }
    return converted;
}

Result<void> ToLegacyResult(const Siligen::SharedKernel::VoidResult& result) {
    if (result.IsSuccess()) {
        return Result<void>::Success();
    }
    const auto& error = result.GetError();
    return Result<void>::Failure(
        Siligen::Shared::Types::Error(
            static_cast<Siligen::Shared::Types::ErrorCode>(static_cast<int>(error.GetCode())),
            error.GetMessage(),
            error.GetModule()));
}

}  // namespace

Result<void> RecipeValidationService::ValidateRecipe(const Recipe& recipe) const {
    Siligen::Process::Recipes::Services::RecipeValidationService service;
    return ToLegacyResult(service.ValidateRecipe(ToNewRecipe(recipe)));
}

Result<void> RecipeValidationService::ValidateRecipeVersion(const RecipeVersion& version,
                                                            const ParameterSchema& schema) const {
    Siligen::Process::Recipes::Services::RecipeValidationService service;
    return ToLegacyResult(service.ValidateRecipeVersion(ToNewRecipeVersion(version), ToNewSchema(schema)));
}

Result<void> RecipeValidationService::ValidateParameters(const RecipeVersion& version,
                                                         const ParameterSchema& schema) const {
    Siligen::Process::Recipes::Services::RecipeValidationService service;
    return ToLegacyResult(service.ValidateRecipeVersion(ToNewRecipeVersion(version), ToNewSchema(schema)));
}

}  // namespace Siligen::Domain::Recipes::DomainServices
