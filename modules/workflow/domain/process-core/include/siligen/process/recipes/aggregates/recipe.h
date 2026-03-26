#pragma once

#include "siligen/process/recipes/value_objects/recipe_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::Process::Recipes::Aggregates {

class Recipe {
   public:
    ValueObjects::RecipeId id;
    std::string name;
    std::string description;
    ValueObjects::RecipeStatus status = ValueObjects::RecipeStatus::Active;
    std::vector<std::string> tags;
    ValueObjects::RecipeVersionId active_version_id;
    std::vector<ValueObjects::RecipeVersionId> version_ids;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;

    [[nodiscard]] bool IsActive() const { return status == ValueObjects::RecipeStatus::Active; }
    [[nodiscard]] bool IsArchived() const { return status == ValueObjects::RecipeStatus::Archived; }
    void Archive() { status = ValueObjects::RecipeStatus::Archived; }
};

}  // namespace Siligen::Process::Recipes::Aggregates
