#pragma once

#include "recipe_lifecycle/domain/recipes/aggregates/Recipe.h"
#include "recipe_lifecycle/domain/recipes/aggregates/RecipeVersion.h"
#include "recipe_lifecycle/domain/recipes/value-objects/ParameterSchema.h"
#include "shared/types/Result.h"

namespace Siligen::Domain::Recipes::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;

class RecipeValidationService {
   public:
    Result<void> ValidateRecipe(const Recipe& recipe) const;
    Result<void> ValidateRecipeVersion(const RecipeVersion& version, const ParameterSchema& schema) const;

   private:
    Result<void> ValidateParameters(const RecipeVersion& version, const ParameterSchema& schema) const;
};

}  // namespace Siligen::Domain::Recipes::DomainServices
