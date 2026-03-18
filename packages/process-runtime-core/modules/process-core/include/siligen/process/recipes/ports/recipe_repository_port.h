#pragma once

#include "siligen/process/recipes/aggregates/recipe.h"
#include "siligen/process/recipes/aggregates/recipe_version.h"
#include "siligen/process/recipes/value_objects/recipe_types.h"
#include "siligen/shared/result.h"

#include <string>
#include <vector>

namespace Siligen::Process::Recipes::Ports {

class RecipeRepositoryPort {
   public:
    virtual ~RecipeRepositoryPort() = default;

    virtual Siligen::SharedKernel::Result<ValueObjects::RecipeId> SaveRecipe(const Aggregates::Recipe& recipe) = 0;
    virtual Siligen::SharedKernel::Result<Aggregates::Recipe> GetRecipeById(const ValueObjects::RecipeId& recipe_id) const = 0;
    virtual Siligen::SharedKernel::Result<Aggregates::Recipe> GetRecipeByName(const std::string& name) const = 0;
    virtual Siligen::SharedKernel::Result<std::vector<Aggregates::Recipe>> ListRecipes(
        const ValueObjects::RecipeListFilter& filter) const = 0;
    virtual Siligen::SharedKernel::VoidResult ArchiveRecipe(const ValueObjects::RecipeId& recipe_id) = 0;

    virtual Siligen::SharedKernel::Result<ValueObjects::RecipeVersionId> SaveVersion(
        const Aggregates::RecipeVersion& version) = 0;
    virtual Siligen::SharedKernel::Result<Aggregates::RecipeVersion> GetVersionById(
        const ValueObjects::RecipeId& recipe_id,
        const ValueObjects::RecipeVersionId& version_id) const = 0;
    virtual Siligen::SharedKernel::Result<std::vector<Aggregates::RecipeVersion>> ListVersions(
        const ValueObjects::RecipeId& recipe_id) const = 0;
};

}  // namespace Siligen::Process::Recipes::Ports
