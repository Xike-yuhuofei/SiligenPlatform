#pragma once

#include "siligen/process/recipes/value_objects/recipe_types.h"
#include "siligen/shared/result.h"

#include <vector>

namespace Siligen::Process::Recipes::Ports {

class AuditRepositoryPort {
   public:
    virtual ~AuditRepositoryPort() = default;

    virtual Siligen::SharedKernel::VoidResult AppendRecord(const ValueObjects::AuditRecord& record) = 0;
    virtual Siligen::SharedKernel::Result<std::vector<ValueObjects::AuditRecord>> ListByRecipe(
        const ValueObjects::RecipeId& recipe_id) const = 0;
    virtual Siligen::SharedKernel::Result<std::vector<ValueObjects::AuditRecord>> ListByVersion(
        const ValueObjects::RecipeId& recipe_id,
        const ValueObjects::RecipeVersionId& version_id) const = 0;
};

}  // namespace Siligen::Process::Recipes::Ports
