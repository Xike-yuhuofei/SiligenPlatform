#pragma once

#include "siligen/process/recipes/value_objects/recipe_types.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Process::Recipes::Aggregates {

class RecipeVersion {
   public:
    ValueObjects::RecipeVersionId id;
    ValueObjects::RecipeId recipe_id;
    std::string version_label;
    ValueObjects::RecipeVersionStatus status = ValueObjects::RecipeVersionStatus::Draft;
    ValueObjects::RecipeVersionId base_version_id;
    std::vector<ValueObjects::ParameterValueEntry> parameters;
    std::string created_by;
    std::int64_t created_at = 0;
    std::optional<std::int64_t> published_at;
    std::string change_note;

    [[nodiscard]] bool IsEditable() const { return status == ValueObjects::RecipeVersionStatus::Draft; }
    [[nodiscard]] bool IsPublished() const { return status == ValueObjects::RecipeVersionStatus::Published; }
    [[nodiscard]] bool IsArchived() const { return status == ValueObjects::RecipeVersionStatus::Archived; }
};

}  // namespace Siligen::Process::Recipes::Aggregates
