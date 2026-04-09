#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/ParameterSchema.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;

class IParameterSchemaPort {
   public:
    virtual ~IParameterSchemaPort() = default;

    virtual Result<ParameterSchema> GetSchemaById(const std::string& schema_id) const = 0;
    virtual Result<ParameterSchema> GetDefaultSchema() const = 0;
    virtual Result<std::vector<ParameterSchema>> ListSchemas() const = 0;
};

}  // namespace Siligen::Domain::Recipes::Ports
