#pragma once

#include "recipe_lifecycle/domain/recipes/aggregates/Recipe.h"
#include "recipe_lifecycle/domain/recipes/aggregates/RecipeVersion.h"
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeListFilter;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;

class IRecipeRepositoryPort {
   public:
    virtual ~IRecipeRepositoryPort() = default;

    virtual Result<RecipeId> SaveRecipe(const Recipe& recipe) = 0;
    virtual Result<Recipe> GetRecipeById(const RecipeId& recipe_id) const = 0;
    virtual Result<Recipe> GetRecipeByName(const std::string& name) const = 0;
    virtual Result<std::vector<Recipe>> ListRecipes(const RecipeListFilter& filter) const = 0;
    virtual Result<void> ArchiveRecipe(const RecipeId& recipe_id) = 0;

    virtual Result<RecipeVersionId> SaveVersion(const RecipeVersion& version) = 0;
    virtual Result<RecipeVersion> GetVersionById(const RecipeId& recipe_id, const RecipeVersionId& version_id) const = 0;
    virtual Result<std::vector<RecipeVersion>> ListVersions(const RecipeId& recipe_id) const = 0;
};

}  // namespace Siligen::Domain::Recipes::Ports
