#pragma once

#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "shared/types/Result.h"

#include "nlohmann/json.hpp"

#include <filesystem>
#include <mutex>
#include <string>

namespace Siligen::Infrastructure::Adapters::Recipes {

class RecipeFileRepository : public Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort {
   public:
    explicit RecipeFileRepository(const std::string& base_directory);
    ~RecipeFileRepository() override = default;

    RecipeFileRepository(const RecipeFileRepository&) = delete;
    RecipeFileRepository& operator=(const RecipeFileRepository&) = delete;
    RecipeFileRepository(RecipeFileRepository&&) = delete;
    RecipeFileRepository& operator=(RecipeFileRepository&&) = delete;

    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::RecipeId> SaveRecipe(
        const Siligen::Domain::Recipes::Aggregates::Recipe& recipe) override;
    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::Aggregates::Recipe> GetRecipeById(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const override;
    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::Aggregates::Recipe> GetRecipeByName(
        const std::string& name) const override;
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recipes::Aggregates::Recipe>> ListRecipes(
        const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& filter) const override;
    Siligen::Shared::Types::Result<void> ArchiveRecipe(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) override;

    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId> SaveVersion(
        const Siligen::Domain::Recipes::Aggregates::RecipeVersion& version) override;
    Siligen::Shared::Types::Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion> GetVersionById(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
        const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id) const override;
    Siligen::Shared::Types::Result<std::vector<Siligen::Domain::Recipes::Aggregates::RecipeVersion>> ListVersions(
        const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const override;

   private:
    std::filesystem::path base_directory_;
    std::filesystem::path recipes_directory_;
    std::filesystem::path versions_directory_;
    mutable std::mutex mutex_;

    Siligen::Shared::Types::Result<void> EnsureDirectories() const;
    Siligen::Shared::Types::Result<void> WriteJsonFile(const std::filesystem::path& path,
                                                       const nlohmann::json& data) const;
    Siligen::Shared::Types::Result<nlohmann::json> ReadJsonFile(const std::filesystem::path& path) const;

    std::filesystem::path RecipePath(const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const;
    std::filesystem::path VersionPath(const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
                                      const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id) const;

    bool RecipeMatchesFilter(const Siligen::Domain::Recipes::Aggregates::Recipe& recipe,
                             const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& filter) const;
};

}  // namespace Siligen::Infrastructure::Adapters::Recipes
