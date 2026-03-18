#include "domain/recipes/domain-services/RecipeActivationService.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;
using Siligen::Domain::Recipes::DomainServices::RecipeActivationService;
using Siligen::Domain::Recipes::Ports::IAuditRepositoryPort;
using Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeRecipeRepository : public IRecipeRepositoryPort {
   public:
    Recipe recipe;
    RecipeVersion version;
    bool save_called = false;
    Recipe saved_recipe;

    Result<RecipeId> SaveRecipe(const Recipe& updated) override {
        save_called = true;
        saved_recipe = updated;
        recipe = updated;
        return Result<RecipeId>::Success(updated.id);
    }

    Result<Recipe> GetRecipeById(const RecipeId& recipe_id) const override {
        if (recipe_id != recipe.id) {
            return Result<Recipe>::Failure(Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found", "FakeRecipeRepository"));
        }
        return Result<Recipe>::Success(recipe);
    }

    Result<Recipe> GetRecipeByName(const std::string& name) const override {
        if (name != recipe.name) {
            return Result<Recipe>::Failure(Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found", "FakeRecipeRepository"));
        }
        return Result<Recipe>::Success(recipe);
    }

    Result<std::vector<Recipe>> ListRecipes(const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& /*filter*/) const override {
        return Result<std::vector<Recipe>>::Success({recipe});
    }

    Result<void> ArchiveRecipe(const RecipeId& recipe_id) override {
        if (recipe_id != recipe.id) {
            return Result<void>::Failure(Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found", "FakeRecipeRepository"));
        }
        recipe.status = RecipeStatus::Archived;
        return Result<void>::Success();
    }

    Result<RecipeVersionId> SaveVersion(const RecipeVersion& updated) override {
        version = updated;
        return Result<RecipeVersionId>::Success(updated.id);
    }

    Result<RecipeVersion> GetVersionById(const RecipeId& recipe_id,
                                         const RecipeVersionId& version_id) const override {
        if (recipe_id != recipe.id || version_id != version.id) {
            return Result<RecipeVersion>::Failure(
                Error(ErrorCode::RECIPE_VERSION_NOT_FOUND, "Recipe version not found", "FakeRecipeRepository"));
        }
        return Result<RecipeVersion>::Success(version);
    }

    Result<std::vector<RecipeVersion>> ListVersions(const RecipeId& recipe_id) const override {
        if (recipe_id != recipe.id) {
            return Result<std::vector<RecipeVersion>>::Failure(
                Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found", "FakeRecipeRepository"));
        }
        return Result<std::vector<RecipeVersion>>::Success({version});
    }

};

class FakeAuditRepository : public IAuditRepositoryPort {
   public:
    std::vector<AuditRecord> records;

    Result<void> AppendRecord(const AuditRecord& record) override {
        records.push_back(record);
        return Result<void>::Success();
    }

    Result<std::vector<AuditRecord>> ListByRecipe(const RecipeId& recipe_id) const override {
        std::vector<AuditRecord> filtered;
        for (const auto& record : records) {
            if (record.recipe_id == recipe_id) {
                filtered.push_back(record);
            }
        }
        return Result<std::vector<AuditRecord>>::Success(filtered);
    }

    Result<std::vector<AuditRecord>> ListByVersion(const RecipeId& recipe_id,
                                                   const RecipeVersionId& version_id) const override {
        std::vector<AuditRecord> filtered;
        for (const auto& record : records) {
            if (record.recipe_id == recipe_id && record.version_id.has_value() &&
                record.version_id.value() == version_id) {
                filtered.push_back(record);
            }
        }
        return Result<std::vector<AuditRecord>>::Success(filtered);
    }
};

Recipe MakeRecipe(const RecipeId& id, const RecipeVersionId& active_version) {
    Recipe recipe;
    recipe.id = id;
    recipe.name = "recipe";
    recipe.status = RecipeStatus::Active;
    recipe.active_version_id = active_version;
    return recipe;
}

RecipeVersion MakeVersion(const RecipeId& recipe_id, const RecipeVersionId& version_id, RecipeVersionStatus status) {
    RecipeVersion version;
    version.id = version_id;
    version.recipe_id = recipe_id;
    version.status = status;
    return version;
}

}  // namespace

TEST(RecipeActivationServiceTest, ReturnsErrorWhenRepositoryMissing) {
    auto service = RecipeActivationService(nullptr, nullptr);
    auto result = service.ActivateVersion("r1", "v1", AuditAction::Rollback, "", 0, "");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(RecipeActivationServiceTest, RejectsArchivedRecipe) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("r1", "v1");
    recipe_repo->recipe.status = RecipeStatus::Archived;
    recipe_repo->version = MakeVersion("r1", "v2", RecipeVersionStatus::Published);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    RecipeActivationService service(recipe_repo, audit_repo);

    auto result = service.ActivateVersion("r1", "v2", AuditAction::Rollback, "user", 123, "audit-1");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::RECIPE_INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "Recipe is archived");
    EXPECT_FALSE(recipe_repo->save_called);
}

TEST(RecipeActivationServiceTest, RejectsUnpublishedVersion) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("r1", "v1");
    recipe_repo->version = MakeVersion("r1", "v2", RecipeVersionStatus::Draft);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    RecipeActivationService service(recipe_repo, audit_repo);

    auto result = service.ActivateVersion("r1", "v2", AuditAction::Rollback, "user", 123, "audit-1");

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::RECIPE_INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "Only published versions can be activated");
    EXPECT_FALSE(recipe_repo->save_called);
}

TEST(RecipeActivationServiceTest, ActivatesVersionAndWritesAudit) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("r1", "v1");
    recipe_repo->version = MakeVersion("r1", "v2", RecipeVersionStatus::Published);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    RecipeActivationService service(recipe_repo, audit_repo);

    auto result = service.ActivateVersion("r1", "v2", AuditAction::Publish, "", 456, "audit-42");

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(recipe_repo->save_called);
    EXPECT_EQ(recipe_repo->saved_recipe.active_version_id, "v2");
    EXPECT_EQ(recipe_repo->saved_recipe.updated_at, 456);

    ASSERT_EQ(audit_repo->records.size(), 1u);
    const auto& record = audit_repo->records.front();
    EXPECT_EQ(record.id, "audit-42");
    EXPECT_EQ(record.recipe_id, "r1");
    ASSERT_TRUE(record.version_id.has_value());
    EXPECT_EQ(record.version_id.value(), "v2");
    EXPECT_EQ(record.action, AuditAction::Publish);
    EXPECT_EQ(record.actor, "system");
    EXPECT_EQ(record.timestamp, 456);
    ASSERT_EQ(record.changes.size(), 1u);
    EXPECT_EQ(record.changes.front().field, "activeVersionId");
    EXPECT_EQ(record.changes.front().before, "");
    EXPECT_EQ(record.changes.front().after, "v2");
}
