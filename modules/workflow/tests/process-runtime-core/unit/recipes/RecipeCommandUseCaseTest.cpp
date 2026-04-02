#include "workflow/application/usecases/recipes/RecipeCommandUseCase.h"

#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::UseCases::Recipes::ActivateRecipeVersionRequest;
using Siligen::Application::UseCases::Recipes::PublishRecipeVersionRequest;
using Siligen::Application::UseCases::Recipes::RecipeCommandUseCase;
using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;
using Siligen::Domain::Recipes::Ports::IAuditRepositoryPort;
using Siligen::Domain::Recipes::Ports::IParameterSchemaPort;
using Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchemaEntry;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueType;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeListFilter;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

class FakeRecipeRepository final : public IRecipeRepositoryPort {
   public:
    Recipe recipe;
    RecipeVersion version;
    bool save_recipe_called = false;
    bool save_version_called = false;
    Recipe saved_recipe;
    RecipeVersion saved_version;

    Result<RecipeId> SaveRecipe(const Recipe& updated) override {
        save_recipe_called = true;
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

    Result<std::vector<Recipe>> ListRecipes(const RecipeListFilter& /*filter*/) const override {
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
        save_version_called = true;
        saved_version = updated;
        version = updated;
        return Result<RecipeVersionId>::Success(updated.id);
    }

    Result<RecipeVersion> GetVersionById(const RecipeId& recipe_id, const RecipeVersionId& version_id) const override {
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

class FakeAuditRepository final : public IAuditRepositoryPort {
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

    Result<std::vector<AuditRecord>> ListByVersion(
        const RecipeId& recipe_id,
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

class FakeParameterSchemaPort final : public IParameterSchemaPort {
   public:
    ParameterSchema default_schema;

    Result<ParameterSchema> GetSchemaById(const std::string& schema_id) const override {
        if (schema_id != default_schema.schema_id) {
            return Result<ParameterSchema>::Failure(
                Error(ErrorCode::PARAMETER_SCHEMA_NOT_FOUND, "Schema not found", "FakeParameterSchemaPort"));
        }
        return Result<ParameterSchema>::Success(default_schema);
    }

    Result<ParameterSchema> GetDefaultSchema() const override {
        return Result<ParameterSchema>::Success(default_schema);
    }

    Result<std::vector<ParameterSchema>> ListSchemas() const override {
        return Result<std::vector<ParameterSchema>>::Success({default_schema});
    }
};

Recipe MakeRecipe(const RecipeId& recipe_id, const RecipeVersionId& active_version_id) {
    Recipe recipe;
    recipe.id = recipe_id;
    recipe.name = "demo-recipe";
    recipe.status = RecipeStatus::Active;
    recipe.active_version_id = active_version_id;
    recipe.version_ids = {active_version_id};
    recipe.created_at = 100;
    recipe.updated_at = 100;
    return recipe;
}

RecipeVersion MakeVersion(
    const RecipeId& recipe_id,
    const RecipeVersionId& version_id,
    RecipeVersionStatus status) {
    RecipeVersion version;
    version.id = version_id;
    version.recipe_id = recipe_id;
    version.version_label = "v1.0";
    version.status = status;
    version.parameters = {{"speed", 12.5}};
    version.created_by = "tester";
    version.created_at = 100;
    return version;
}

ParameterSchema MakeSchema() {
    ParameterSchema schema;
    schema.schema_id = "default";

    ParameterSchemaEntry speed;
    speed.key = "speed";
    speed.display_name = "Speed";
    speed.value_type = ParameterValueType::Float;
    speed.required = true;
    speed.constraints.min_value = 0.0;
    speed.constraints.max_value = 100.0;

    schema.entries.push_back(speed);
    return schema;
}

}  // namespace

TEST(RecipeCommandUseCaseTest, ActivateRecipeVersionDelegatesToProcessCore) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("recipe-1", "version-1");
    recipe_repo->version = MakeVersion("recipe-1", "version-2", RecipeVersionStatus::Published);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    auto schema_port = std::make_shared<FakeParameterSchemaPort>();
    schema_port->default_schema = MakeSchema();

    RecipeCommandUseCase use_case(recipe_repo, audit_repo, schema_port);

    ActivateRecipeVersionRequest request;
    request.recipe_id = "recipe-1";
    request.version_id = "version-2";
    request.actor = "operator-a";

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value().active_version_id, "version-2");
    EXPECT_TRUE(recipe_repo->save_recipe_called);
    EXPECT_EQ(recipe_repo->saved_recipe.active_version_id, "version-2");

    ASSERT_EQ(audit_repo->records.size(), 1u);
    EXPECT_EQ(audit_repo->records.front().action, AuditAction::Rollback);
    ASSERT_TRUE(audit_repo->records.front().version_id.has_value());
    EXPECT_EQ(audit_repo->records.front().version_id.value(), "version-2");
}

TEST(RecipeCommandUseCaseTest, PublishRecipeVersionValidatesAndActivatesThroughProcessCore) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("recipe-1", "version-1");
    recipe_repo->version = MakeVersion("recipe-1", "version-2", RecipeVersionStatus::Draft);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    auto schema_port = std::make_shared<FakeParameterSchemaPort>();
    schema_port->default_schema = MakeSchema();

    RecipeCommandUseCase use_case(recipe_repo, audit_repo, schema_port);

    PublishRecipeVersionRequest request;
    request.recipe_id = "recipe-1";
    request.version_id = "version-2";
    request.actor = "publisher";

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(recipe_repo->save_version_called);
    EXPECT_EQ(recipe_repo->saved_version.status, RecipeVersionStatus::Published);
    ASSERT_TRUE(recipe_repo->saved_version.published_at.has_value());
    EXPECT_TRUE(recipe_repo->save_recipe_called);
    EXPECT_EQ(recipe_repo->saved_recipe.active_version_id, "version-2");

    ASSERT_EQ(audit_repo->records.size(), 1u);
    EXPECT_EQ(audit_repo->records.front().action, AuditAction::Publish);
    EXPECT_EQ(audit_repo->records.front().actor, "publisher");
}
