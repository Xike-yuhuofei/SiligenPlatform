#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::Aggregates {

class Recipe {
   public:
    ValueObjects::RecipeId id;
    std::string name;
    std::string description;
    ValueObjects::RecipeStatus status;
    std::vector<std::string> tags;
    ValueObjects::RecipeVersionId active_version_id;
    std::vector<ValueObjects::RecipeVersionId> version_ids;
    std::int64_t created_at;
    std::int64_t updated_at;

    Recipe()
        : status(ValueObjects::RecipeStatus::Active),
          created_at(0),
          updated_at(0) {}

    bool IsActive() const {
        return status == ValueObjects::RecipeStatus::Active;
    }

    bool IsArchived() const {
        return status == ValueObjects::RecipeStatus::Archived;
    }

    void Archive() {
        status = ValueObjects::RecipeStatus::Archived;
    }
};

}  // namespace Siligen::Domain::Recipes::Aggregates
