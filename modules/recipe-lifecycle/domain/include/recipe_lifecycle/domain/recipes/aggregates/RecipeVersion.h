#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::Aggregates {

class RecipeVersion {
   public:
    ValueObjects::RecipeVersionId id;
    ValueObjects::RecipeId recipe_id;
    std::string version_label;
    ValueObjects::RecipeVersionStatus status;
    ValueObjects::RecipeVersionId base_version_id;
    std::vector<ValueObjects::ParameterValueEntry> parameters;
    std::string created_by;
    std::int64_t created_at;
    std::optional<std::int64_t> published_at;
    std::string change_note;

    RecipeVersion()
        : status(ValueObjects::RecipeVersionStatus::Draft),
          created_at(0) {}

    bool IsEditable() const {
        return status == ValueObjects::RecipeVersionStatus::Draft;
    }

    bool IsPublished() const {
        return status == ValueObjects::RecipeVersionStatus::Published;
    }

    bool IsArchived() const {
        return status == ValueObjects::RecipeVersionStatus::Archived;
    }
};

}  // namespace Siligen::Domain::Recipes::Aggregates
