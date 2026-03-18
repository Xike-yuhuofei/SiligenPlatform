#pragma once

#include "siligen/process/recipes/aggregates/recipe.h"
#include "siligen/process/recipes/aggregates/recipe_version.h"
#include "siligen/process/recipes/value_objects/parameter_schema.h"
#include "siligen/shared/result.h"

namespace Siligen::Process::Recipes::Services {

class RecipeValidationService {
   public:
    Siligen::SharedKernel::VoidResult ValidateRecipe(const Aggregates::Recipe& recipe) const;
    Siligen::SharedKernel::VoidResult ValidateRecipeVersion(
        const Aggregates::RecipeVersion& version,
        const ValueObjects::ParameterSchema& schema) const;

   private:
    Siligen::SharedKernel::VoidResult ValidateParameters(
        const Aggregates::RecipeVersion& version,
        const ValueObjects::ParameterSchema& schema) const;
};

}  // namespace Siligen::Process::Recipes::Services
