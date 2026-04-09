#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <vector>

namespace Siligen::Domain::Recipes::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;

class IAuditRepositoryPort {
   public:
    virtual ~IAuditRepositoryPort() = default;

    virtual Result<void> AppendRecord(const AuditRecord& record) = 0;
    virtual Result<std::vector<AuditRecord>> ListByRecipe(const RecipeId& recipe_id) const = 0;
    virtual Result<std::vector<AuditRecord>> ListByVersion(const RecipeId& recipe_id,
                                                           const RecipeVersionId& version_id) const = 0;
};

}  // namespace Siligen::Domain::Recipes::Ports
