#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeBundle.h"
#include "shared/types/Result.h"

#include <string>

namespace Siligen::Domain::Recipes::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::RecipeBundle;

class IRecipeBundleSerializerPort {
   public:
    virtual ~IRecipeBundleSerializerPort() = default;

    virtual Result<std::string> Serialize(const RecipeBundle& bundle) = 0;
    virtual Result<RecipeBundle> Deserialize(const std::string& payload) = 0;
};

}  // namespace Siligen::Domain::Recipes::Ports
