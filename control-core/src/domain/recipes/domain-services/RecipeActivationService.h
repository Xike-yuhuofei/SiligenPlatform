#pragma once

#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <cstdint>
#include <memory>
#include <string>
namespace Siligen::Domain::Recipes::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecordId;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;

struct ActivateRecipeVersionResult {
    RecipeId recipe_id;
    RecipeVersionId active_version_id;
};

class RecipeActivationService {
   public:
    RecipeActivationService(
        std::shared_ptr<Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Ports::IAuditRepositoryPort> audit_repository) noexcept;

    Result<ActivateRecipeVersionResult> ActivateVersion(
        const RecipeId& recipe_id,
        const RecipeVersionId& version_id,
        AuditAction audit_action,
        const std::string& actor,
        std::int64_t timestamp,
        const AuditRecordId& audit_id) noexcept;

   private:
    std::shared_ptr<Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Ports::IAuditRepositoryPort> audit_repository_;
};

}  // namespace Siligen::Domain::Recipes::DomainServices
