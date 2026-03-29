#pragma once

#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "shared/types/Result.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace Siligen::Infrastructure::Adapters::Recipes {

class AuditFileRepository : public Siligen::Domain::Recipes::Ports::IAuditRepositoryPort {
   public:
    explicit AuditFileRepository(const std::string& base_directory);
    ~AuditFileRepository() override = default;

    AuditFileRepository(const AuditFileRepository&) = delete;
    AuditFileRepository& operator=(const AuditFileRepository&) = delete;
    AuditFileRepository(AuditFileRepository&&) = delete;
    AuditFileRepository& operator=(AuditFileRepository&&) = delete;

    Siligen::Shared::Types::Result<void> AppendRecord(
        const Siligen::Domain::Recipes::ValueObjects::AuditRecord& record) override;
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recipes::ValueObjects::AuditRecord>> ListByRecipe(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const override;
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recipes::ValueObjects::AuditRecord>> ListByVersion(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
        const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id) const override;

   private:
    std::filesystem::path base_directory_;
    std::filesystem::path audit_directory_;
    mutable std::mutex mutex_;

    Siligen::Shared::Types::Result<void> EnsureDirectory() const;
    std::filesystem::path AuditPath(const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const;
};

}  // namespace Siligen::Infrastructure::Adapters::Recipes
