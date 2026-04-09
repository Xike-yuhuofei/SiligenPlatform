#include "recipe_lifecycle/domain/recipes/domain-services/RecipeValidationService.h"

#include "shared/types/Error.h"

#include <unordered_map>

namespace Siligen::Domain::Recipes::DomainServices {
namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchemaEntry;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueType;

Result<void> MakeValidationError(const std::string& message) {
    return Result<void>::Failure(
        Error(ErrorCode::RECIPE_VALIDATION_FAILED, message, "RecipeValidationService"));
}

const ParameterSchemaEntry* FindSchemaEntry(
    const std::vector<ParameterSchemaEntry>& entries,
    const std::string& key) {
    for (const auto& entry : entries) {
        if (entry.key == key) {
            return &entry;
        }
    }
    return nullptr;
}

}  // namespace

Result<void> RecipeValidationService::ValidateRecipe(const Recipe& recipe) const {
    if (recipe.name.empty()) {
        return MakeValidationError("Recipe name is required");
    }
    if (recipe.name.size() > 64) {
        return MakeValidationError("Recipe name must be 1-64 characters");
    }
    return Result<void>::Success();
}

Result<void> RecipeValidationService::ValidateRecipeVersion(
    const RecipeVersion& version,
    const ParameterSchema& schema) const {
    if (version.recipe_id.empty()) {
        return MakeValidationError("Recipe version must reference a recipe id");
    }
    if (version.version_label.empty()) {
        return MakeValidationError("Recipe version label is required");
    }
    return ValidateParameters(version, schema);
}

Result<void> RecipeValidationService::ValidateParameters(
    const RecipeVersion& version,
    const ParameterSchema& schema) const {
    std::unordered_map<std::string, const ValueObjects::ParameterValueEntry*> values;
    values.reserve(version.parameters.size());

    for (const auto& param : version.parameters) {
        if (param.key.empty()) {
            return MakeValidationError("Parameter key is required");
        }
        if (values.find(param.key) != values.end()) {
            return MakeValidationError("Duplicate parameter key: " + param.key);
        }
        values[param.key] = &param;
    }

    for (const auto& entry : schema.entries) {
        if (entry.required && values.find(entry.key) == values.end()) {
            return MakeValidationError("Missing required parameter: " + entry.key);
        }
    }

    for (const auto& pair : values) {
        const auto* schema_entry = FindSchemaEntry(schema.entries, pair.first);
        if (schema_entry == nullptr) {
            return MakeValidationError("Unknown parameter key: " + pair.first);
        }

        const auto& param = *pair.second;
        switch (schema_entry->value_type) {
            case ParameterValueType::Integer: {
                if (!std::holds_alternative<std::int64_t>(param.value)) {
                    return MakeValidationError("Parameter type mismatch (integer): " + param.key);
                }
                const auto value = static_cast<double>(std::get<std::int64_t>(param.value));
                if (schema_entry->constraints.min_value.has_value() &&
                    value < schema_entry->constraints.min_value.value()) {
                    return MakeValidationError("Parameter below min: " + param.key);
                }
                if (schema_entry->constraints.max_value.has_value() &&
                    value > schema_entry->constraints.max_value.value()) {
                    return MakeValidationError("Parameter above max: " + param.key);
                }
                break;
            }
            case ParameterValueType::Float: {
                if (!std::holds_alternative<double>(param.value)) {
                    return MakeValidationError("Parameter type mismatch (float): " + param.key);
                }
                const auto value = std::get<double>(param.value);
                if (schema_entry->constraints.min_value.has_value() &&
                    value < schema_entry->constraints.min_value.value()) {
                    return MakeValidationError("Parameter below min: " + param.key);
                }
                if (schema_entry->constraints.max_value.has_value() &&
                    value > schema_entry->constraints.max_value.value()) {
                    return MakeValidationError("Parameter above max: " + param.key);
                }
                break;
            }
            case ParameterValueType::Boolean:
                if (!std::holds_alternative<bool>(param.value)) {
                    return MakeValidationError("Parameter type mismatch (bool): " + param.key);
                }
                break;
            case ParameterValueType::String:
                if (!std::holds_alternative<std::string>(param.value)) {
                    return MakeValidationError("Parameter type mismatch (string): " + param.key);
                }
                break;
            case ParameterValueType::Enum: {
                if (!std::holds_alternative<std::string>(param.value)) {
                    return MakeValidationError("Parameter type mismatch (enum): " + param.key);
                }
                if (!schema_entry->constraints.allowed_values.empty()) {
                    const auto& value = std::get<std::string>(param.value);
                    bool found = false;
                    for (const auto& allowed : schema_entry->constraints.allowed_values) {
                        if (allowed == value) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return MakeValidationError("Parameter value not allowed: " + param.key);
                    }
                }
                break;
            }
        }
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Recipes::DomainServices
