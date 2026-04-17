#include "recipe_lifecycle/application/usecases/recipes/CreateDraftVersionUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/CreateVersionFromPublishedUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/RecipeCommandUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/UpdateDraftVersionUseCase.h"

#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::UseCases::Recipes::CreateDraftVersionRequest;
using Siligen::Application::UseCases::Recipes::CreateDraftVersionUseCase;
using Siligen::Application::UseCases::Recipes::CreateVersionFromPublishedRequest;
using Siligen::Application::UseCases::Recipes::CreateVersionFromPublishedUseCase;
using Siligen::Application::UseCases::Recipes::ActivateRecipeVersionRequest;
using Siligen::Application::UseCases::Recipes::PublishRecipeVersionRequest;
using Siligen::Application::UseCases::Recipes::RecipeCommandUseCase;
using Siligen::Application::UseCases::Recipes::UpdateDraftVersionRequest;
using Siligen::Application::UseCases::Recipes::UpdateDraftVersionUseCase;
using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;
using Siligen::Domain::Recipes::Ports::IAuditRepositoryPort;
using Siligen::Domain::Recipes::Ports::IParameterSchemaPort;
using Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort;
using Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchemaEntry;
using Siligen::Domain::Recipes::ValueObjects::ParameterValue;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueType;
using Siligen::Domain::Recipes::ValueObjects::RecipeId;
using Siligen::Domain::Recipes::ValueObjects::RecipeListFilter;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionId;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Domain::Recipes::ValueObjects::Template;
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

class FakeTemplateRepository final : public ITemplateRepositoryPort {
   public:
    Template template_data;

    Result<Siligen::Domain::Recipes::ValueObjects::TemplateId> SaveTemplate(const Template& updated) override {
        template_data = updated;
        return Result<Siligen::Domain::Recipes::ValueObjects::TemplateId>::Success(updated.id);
    }

    Result<Template> GetTemplateById(const Siligen::Domain::Recipes::ValueObjects::TemplateId& template_id) const override {
        if (template_id != template_data.id) {
            return Result<Template>::Failure(Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found", "FakeTemplateRepository"));
        }
        return Result<Template>::Success(template_data);
    }

    Result<Template> GetTemplateByName(const std::string& name) const override {
        if (name != template_data.name) {
            return Result<Template>::Failure(Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found", "FakeTemplateRepository"));
        }
        return Result<Template>::Success(template_data);
    }

    Result<std::vector<Template>> ListTemplates() const override {
        return Result<std::vector<Template>>::Success({template_data});
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

    ParameterSchemaEntry trigger_interval;
    trigger_interval.key = "trigger_spatial_interval_mm";
    trigger_interval.display_name = "Trigger Spatial Interval";
    trigger_interval.value_type = ParameterValueType::Float;
    trigger_interval.required = true;
    trigger_interval.constraints.min_value = 0.1;
    trigger_interval.constraints.max_value = 1000.0;
    schema.entries.push_back(trigger_interval);

    ParameterSchemaEntry direction_mode;
    direction_mode.key = "point_flying_direction_mode";
    direction_mode.display_name = "Point Flying Direction Mode";
    direction_mode.value_type = ParameterValueType::Enum;
    direction_mode.required = true;
    direction_mode.constraints.allowed_values = {"approach_direction"};
    schema.entries.push_back(direction_mode);

    return schema;
}

Template MakeTemplate(const std::string& template_id) {
    Template template_data;
    template_data.id = template_id;
    template_data.name = "demo-template";
    template_data.description = "template";
    template_data.created_at = 100;
    template_data.updated_at = 100;
    template_data.parameters = {{"speed", 12.5}};
    return template_data;
}

const ParameterValueEntry* FindParameter(const std::vector<ParameterValueEntry>& parameters, const std::string& key) {
    auto it = std::find_if(
        parameters.begin(),
        parameters.end(),
        [&key](const ParameterValueEntry& entry) { return entry.key == key; });
    return it == parameters.end() ? nullptr : &(*it);
}

void ExpectPlanningPolicyDefaults(const std::vector<ParameterValueEntry>& parameters) {
    const auto* interval = FindParameter(parameters, "trigger_spatial_interval_mm");
    ASSERT_NE(interval, nullptr);
    ASSERT_TRUE(std::holds_alternative<double>(interval->value));
    EXPECT_DOUBLE_EQ(std::get<double>(interval->value), 3.0);

    const auto* direction = FindParameter(parameters, "point_flying_direction_mode");
    ASSERT_NE(direction, nullptr);
    ASSERT_TRUE(std::holds_alternative<std::string>(direction->value));
    EXPECT_EQ(std::get<std::string>(direction->value), "approach_direction");
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
    ExpectPlanningPolicyDefaults(recipe_repo->saved_version.parameters);
    EXPECT_TRUE(recipe_repo->save_recipe_called);
    EXPECT_EQ(recipe_repo->saved_recipe.active_version_id, "version-2");

    ASSERT_EQ(audit_repo->records.size(), 1u);
    EXPECT_EQ(audit_repo->records.front().action, AuditAction::Publish);
    EXPECT_EQ(audit_repo->records.front().actor, "publisher");
}

TEST(RecipeCommandUseCaseTest, CreateDraftVersionAddsPlanningPolicyDefaultsForTemplateDraft) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("recipe-1", "version-1");

    auto template_repo = std::make_shared<FakeTemplateRepository>();
    template_repo->template_data = MakeTemplate("template-1");

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    auto schema_port = std::make_shared<FakeParameterSchemaPort>();
    schema_port->default_schema = MakeSchema();

    CreateDraftVersionUseCase use_case(recipe_repo, template_repo, schema_port, audit_repo);

    CreateDraftVersionRequest request;
    request.recipe_id = "recipe-1";
    request.template_id = "template-1";
    request.created_by = "planner";

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(recipe_repo->save_version_called);
    ExpectPlanningPolicyDefaults(recipe_repo->saved_version.parameters);
}

TEST(RecipeCommandUseCaseTest, CreateVersionFromPublishedAddsPlanningPolicyDefaultsFromLegacyBaseVersion) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("recipe-1", "version-1");
    recipe_repo->version = MakeVersion("recipe-1", "version-1", RecipeVersionStatus::Published);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    auto schema_port = std::make_shared<FakeParameterSchemaPort>();
    schema_port->default_schema = MakeSchema();

    CreateVersionFromPublishedUseCase use_case(recipe_repo, schema_port, audit_repo);

    CreateVersionFromPublishedRequest request;
    request.recipe_id = "recipe-1";
    request.base_version_id = "version-1";
    request.created_by = "planner";

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(recipe_repo->save_version_called);
    ExpectPlanningPolicyDefaults(recipe_repo->saved_version.parameters);
}

TEST(RecipeCommandUseCaseTest, UpdateDraftVersionAddsPlanningPolicyDefaultsBeforeValidation) {
    auto recipe_repo = std::make_shared<FakeRecipeRepository>();
    recipe_repo->recipe = MakeRecipe("recipe-1", "version-1");
    recipe_repo->version = MakeVersion("recipe-1", "version-2", RecipeVersionStatus::Draft);

    auto audit_repo = std::make_shared<FakeAuditRepository>();
    auto schema_port = std::make_shared<FakeParameterSchemaPort>();
    schema_port->default_schema = MakeSchema();

    UpdateDraftVersionUseCase use_case(recipe_repo, schema_port, audit_repo);

    UpdateDraftVersionRequest request;
    request.recipe_id = "recipe-1";
    request.version_id = "version-2";
    request.editor = "editor";
    request.parameters = {{"speed", 18.0}};

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(recipe_repo->save_version_called);
    ExpectPlanningPolicyDefaults(recipe_repo->saved_version.parameters);
}
