#pragma once

#include "recipe_lifecycle/domain/recipes/ports/IRecipeBundleSerializerPort.h"
#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeBundle.h"

#include <string>

namespace Siligen::Infrastructure::Adapters::Recipes {

class RecipeBundleSerializer : public Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort {
   public:
    using RecipeJsonSerializer = Siligen::Domain::Recipes::Serialization::RecipeJsonSerializer;
    using json = RecipeJsonSerializer::json;
    using RecipeBundle = Siligen::Domain::Recipes::ValueObjects::RecipeBundle;

    Siligen::Shared::Types::Result<std::string> Serialize(const RecipeBundle& bundle) override;
    Siligen::Shared::Types::Result<RecipeBundle> Deserialize(const std::string& payload) override;

    static json BundleToJson(const RecipeBundle& bundle);
    static Siligen::Shared::Types::Result<RecipeBundle> BundleFromJson(const json& value);
};

}  // namespace Siligen::Infrastructure::Adapters::Recipes
