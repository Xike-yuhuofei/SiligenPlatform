#pragma once

#include "siligen/process/recipes/ports/audit_repository_port.h"
#include "siligen/process/recipes/ports/recipe_repository_port.h"
#include "siligen/process/recipes/value_objects/recipe_types.h"
#include "siligen/shared/result.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Siligen::Process::Recipes::Services {

struct ActivateRecipeVersionResult {
    ValueObjects::RecipeId recipe_id;
    ValueObjects::RecipeVersionId active_version_id;
};

class RecipeActivationService {
   public:
    RecipeActivationService(
        std::shared_ptr<Ports::RecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Ports::AuditRepositoryPort> audit_repository) noexcept;

    Siligen::SharedKernel::Result<ActivateRecipeVersionResult> ActivateVersion(
        const ValueObjects::RecipeId& recipe_id,
        const ValueObjects::RecipeVersionId& version_id,
        ValueObjects::AuditAction audit_action,
        const std::string& actor,
        std::int64_t timestamp,
        const ValueObjects::AuditRecordId& audit_id) noexcept;

   private:
    std::shared_ptr<Ports::RecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Ports::AuditRepositoryPort> audit_repository_;

    static std::vector<ValueObjects::FieldChange> DiffStrings(
        const std::string& field,
        const std::string& before,
        const std::string& after) noexcept;
};

}  // namespace Siligen::Process::Recipes::Services
