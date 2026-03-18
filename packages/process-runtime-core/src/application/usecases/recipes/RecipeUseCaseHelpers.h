#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/value-objects/ParameterSchema.h"
#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "siligen/process/recipes/ports/audit_repository_port.h"
#include "siligen/process/recipes/ports/recipe_repository_port.h"
#include "siligen/process/recipes/services/recipe_activation_service.h"
#include "siligen/process/recipes/services/recipe_validation_service.h"

#include <chrono>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct ActivateRecipeVersionResultData {
    std::string recipe_id;
    std::string active_version_id;
};

inline std::int64_t NowEpochMillis() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

inline std::string GenerateId(const std::string& prefix) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::ostringstream oss;
    oss << prefix << "-";
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << dis(gen);
    }
    oss << "-";
    for (int i = 0; i < 4; ++i) {
        oss << std::hex << dis(gen);
    }
    return oss.str();
}

inline std::string ParameterValueToString(const Siligen::Domain::Recipes::ValueObjects::ParameterValue& value) {
    return std::visit(
        [](auto&& arg) -> std::string {
            std::ostringstream oss;
            oss << std::boolalpha << arg;
            return oss.str();
        },
        value);
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffParameters(
    const std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>& before,
    const std::vector<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>& after) {
    using Siligen::Domain::Recipes::ValueObjects::FieldChange;

    std::unordered_map<std::string, Siligen::Domain::Recipes::ValueObjects::ParameterValue> before_map;
    std::unordered_map<std::string, Siligen::Domain::Recipes::ValueObjects::ParameterValue> after_map;
    for (const auto& entry : before) {
        before_map[entry.key] = entry.value;
    }
    for (const auto& entry : after) {
        after_map[entry.key] = entry.value;
    }

    std::vector<FieldChange> changes;
    for (const auto& pair : before_map) {
        const auto& key = pair.first;
        auto it = after_map.find(key);
        if (it == after_map.end()) {
            FieldChange change;
            change.field = "parameters." + key;
            change.before = ParameterValueToString(pair.second);
            change.after = "<removed>";
            changes.push_back(change);
        } else if (ParameterValueToString(pair.second) != ParameterValueToString(it->second)) {
            FieldChange change;
            change.field = "parameters." + key;
            change.before = ParameterValueToString(pair.second);
            change.after = ParameterValueToString(it->second);
            changes.push_back(change);
        }
    }
    for (const auto& pair : after_map) {
        if (before_map.find(pair.first) == before_map.end()) {
            FieldChange change;
            change.field = "parameters." + pair.first;
            change.before = "<added>";
            change.after = ParameterValueToString(pair.second);
            changes.push_back(change);
        }
    }
    return changes;
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffStrings(
    const std::string& field,
    const std::string& before,
    const std::string& after) {
    if (before == after) {
        return {};
    }
    Siligen::Domain::Recipes::ValueObjects::FieldChange change;
    change.field = field;
    change.before = before;
    change.after = after;
    return {change};
}

inline std::vector<Siligen::Domain::Recipes::ValueObjects::FieldChange> DiffTags(
    const std::vector<std::string>& before,
    const std::vector<std::string>& after) {
    auto join = [](const std::vector<std::string>& values) {
        std::ostringstream oss;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                oss << ",";
            }
            oss << values[i];
        }
        return oss.str();
    };
    return DiffStrings("tags", join(before), join(after));
}

inline Siligen::Process::Recipes::ValueObjects::ParameterSchema ToNewSchema(
    const Siligen::Domain::Recipes::ValueObjects::ParameterSchema& schema) {
    Siligen::Process::Recipes::ValueObjects::ParameterSchema converted;
    converted.schema_id = schema.schema_id;
    converted.entries.reserve(schema.entries.size());
    for (const auto& entry : schema.entries) {
        Siligen::Process::Recipes::ValueObjects::ParameterSchemaEntry new_entry;
        new_entry.key = entry.key;
        new_entry.display_name = entry.display_name;
        new_entry.value_type =
            static_cast<Siligen::Process::Recipes::ValueObjects::ParameterValueType>(entry.value_type);
        new_entry.unit = entry.unit;
        new_entry.constraints.min_value = entry.constraints.min_value;
        new_entry.constraints.max_value = entry.constraints.max_value;
        new_entry.constraints.allowed_values = entry.constraints.allowed_values;
        new_entry.required = entry.required;
        converted.entries.push_back(std::move(new_entry));
    }
    return converted;
}

inline Siligen::Process::Recipes::Aggregates::Recipe ToNewRecipe(
    const Siligen::Domain::Recipes::Aggregates::Recipe& recipe) {
    Siligen::Process::Recipes::Aggregates::Recipe converted;
    converted.id = recipe.id;
    converted.name = recipe.name;
    converted.description = recipe.description;
    converted.status = static_cast<Siligen::Process::Recipes::ValueObjects::RecipeStatus>(recipe.status);
    converted.tags = recipe.tags;
    converted.active_version_id = recipe.active_version_id;
    converted.version_ids = recipe.version_ids;
    converted.created_at = recipe.created_at;
    converted.updated_at = recipe.updated_at;
    return converted;
}

inline Siligen::Process::Recipes::Aggregates::RecipeVersion ToNewRecipeVersion(
    const Siligen::Domain::Recipes::Aggregates::RecipeVersion& version) {
    Siligen::Process::Recipes::Aggregates::RecipeVersion converted;
    converted.id = version.id;
    converted.recipe_id = version.recipe_id;
    converted.version_label = version.version_label;
    converted.status =
        static_cast<Siligen::Process::Recipes::ValueObjects::RecipeVersionStatus>(version.status);
    converted.base_version_id = version.base_version_id;
    converted.parameters.reserve(version.parameters.size());
    for (const auto& param : version.parameters) {
        converted.parameters.push_back({param.key, param.value});
    }
    converted.created_by = version.created_by;
    converted.created_at = version.created_at;
    converted.published_at = version.published_at;
    converted.change_note = version.change_note;
    return converted;
}

inline Siligen::Shared::Types::Result<void> ToLegacyVoidResult(const Siligen::SharedKernel::VoidResult& result) {
    if (result.IsSuccess()) {
        return Siligen::Shared::Types::Result<void>::Success();
    }
    const auto& error = result.GetError();
    return Siligen::Shared::Types::Result<void>::Failure(
        Siligen::Shared::Types::Error(
            static_cast<Siligen::Shared::Types::ErrorCode>(static_cast<int>(error.GetCode())),
            error.GetMessage(),
            error.GetModule()));
}

inline Siligen::Shared::Types::Error ToLegacyError(const Siligen::SharedKernel::Error& error) {
    return Siligen::Shared::Types::Error(
        static_cast<Siligen::Shared::Types::ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

inline Siligen::SharedKernel::Error ToNewError(const Siligen::Shared::Types::Error& error) {
    return Siligen::SharedKernel::Error(
        static_cast<Siligen::SharedKernel::ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

inline Siligen::SharedKernel::VoidResult ToNewVoidResult(const Siligen::Shared::Types::Result<void>& result) {
    if (result.IsSuccess()) {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    return Siligen::SharedKernel::VoidResult::Failure(ToNewError(result.GetError()));
}

inline Siligen::Domain::Recipes::Aggregates::Recipe ToLegacyRecipe(
    const Siligen::Process::Recipes::Aggregates::Recipe& recipe) {
    Siligen::Domain::Recipes::Aggregates::Recipe converted;
    converted.id = recipe.id;
    converted.name = recipe.name;
    converted.description = recipe.description;
    converted.status = static_cast<Siligen::Domain::Recipes::ValueObjects::RecipeStatus>(recipe.status);
    converted.tags = recipe.tags;
    converted.active_version_id = recipe.active_version_id;
    converted.version_ids = recipe.version_ids;
    converted.created_at = recipe.created_at;
    converted.updated_at = recipe.updated_at;
    return converted;
}

inline Siligen::Domain::Recipes::Aggregates::RecipeVersion ToLegacyRecipeVersion(
    const Siligen::Process::Recipes::Aggregates::RecipeVersion& version) {
    Siligen::Domain::Recipes::Aggregates::RecipeVersion converted;
    converted.id = version.id;
    converted.recipe_id = version.recipe_id;
    converted.version_label = version.version_label;
    converted.status =
        static_cast<Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus>(version.status);
    converted.base_version_id = version.base_version_id;
    converted.parameters.reserve(version.parameters.size());
    for (const auto& param : version.parameters) {
        converted.parameters.push_back({param.key, param.value});
    }
    converted.created_by = version.created_by;
    converted.created_at = version.created_at;
    converted.published_at = version.published_at;
    converted.change_note = version.change_note;
    return converted;
}

inline Siligen::Process::Recipes::ValueObjects::RecipeListFilter ToNewRecipeListFilter(
    const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& filter) {
    Siligen::Process::Recipes::ValueObjects::RecipeListFilter converted;
    if (filter.status.has_value()) {
        converted.status =
            static_cast<Siligen::Process::Recipes::ValueObjects::RecipeStatus>(filter.status.value());
    }
    converted.query = filter.query;
    converted.tag = filter.tag;
    return converted;
}

inline Siligen::Domain::Recipes::ValueObjects::RecipeListFilter ToLegacyRecipeListFilter(
    const Siligen::Process::Recipes::ValueObjects::RecipeListFilter& filter) {
    Siligen::Domain::Recipes::ValueObjects::RecipeListFilter converted;
    if (filter.status.has_value()) {
        converted.status =
            static_cast<Siligen::Domain::Recipes::ValueObjects::RecipeStatus>(filter.status.value());
    }
    converted.query = filter.query;
    converted.tag = filter.tag;
    return converted;
}

inline Siligen::Domain::Recipes::ValueObjects::AuditRecord ToLegacyAuditRecord(
    const Siligen::Process::Recipes::ValueObjects::AuditRecord& record) {
    Siligen::Domain::Recipes::ValueObjects::AuditRecord converted;
    converted.id = record.id;
    converted.recipe_id = record.recipe_id;
    converted.version_id = record.version_id;
    converted.action = static_cast<Siligen::Domain::Recipes::ValueObjects::AuditAction>(record.action);
    converted.actor = record.actor;
    converted.timestamp = record.timestamp;
    converted.changes.reserve(record.changes.size());
    for (const auto& change : record.changes) {
        converted.changes.push_back({change.field, change.before, change.after});
    }
    return converted;
}

inline Siligen::Process::Recipes::ValueObjects::AuditRecord ToNewAuditRecord(
    const Siligen::Domain::Recipes::ValueObjects::AuditRecord& record) {
    Siligen::Process::Recipes::ValueObjects::AuditRecord converted;
    converted.id = record.id;
    converted.recipe_id = record.recipe_id;
    converted.version_id = record.version_id;
    converted.action = static_cast<Siligen::Process::Recipes::ValueObjects::AuditAction>(record.action);
    converted.actor = record.actor;
    converted.timestamp = record.timestamp;
    converted.changes.reserve(record.changes.size());
    for (const auto& change : record.changes) {
        converted.changes.push_back({change.field, change.before, change.after});
    }
    return converted;
}

class LegacyRecipeRepositoryAdapter final : public Siligen::Process::Recipes::Ports::RecipeRepositoryPort {
   public:
    explicit LegacyRecipeRepositoryAdapter(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> repository) noexcept
        : repository_(std::move(repository)) {}

    Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeId> SaveRecipe(
        const Siligen::Process::Recipes::Aggregates::Recipe& recipe) override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeId>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->SaveRecipe(ToLegacyRecipe(recipe));
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeId>::Success(result.Value());
        }
        return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeId>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe> GetRecipeById(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->GetRecipeById(recipe_id);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Success(
                ToNewRecipe(result.Value()));
        }
        return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe> GetRecipeByName(
        const std::string& name) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->GetRecipeByName(name);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Success(
                ToNewRecipe(result.Value()));
        }
        return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::Recipe>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::Recipe>> ListRecipes(
        const Siligen::Process::Recipes::ValueObjects::RecipeListFilter& filter) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::Recipe>>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->ListRecipes(ToLegacyRecipeListFilter(filter));
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::Recipe>>::Failure(
                ToNewError(result.GetError()));
        }
        std::vector<Siligen::Process::Recipes::Aggregates::Recipe> converted;
        converted.reserve(result.Value().size());
        for (const auto& recipe : result.Value()) {
            converted.push_back(ToNewRecipe(recipe));
        }
        return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::Recipe>>::Success(
            std::move(converted));
    }

    Siligen::SharedKernel::VoidResult ArchiveRecipe(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id) override {
        if (!repository_) {
            return Siligen::SharedKernel::VoidResult::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        return ToNewVoidResult(repository_->ArchiveRecipe(recipe_id));
    }

    Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeVersionId> SaveVersion(
        const Siligen::Process::Recipes::Aggregates::RecipeVersion& version) override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeVersionId>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->SaveVersion(ToLegacyRecipeVersion(version));
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeVersionId>::Success(
                result.Value());
        }
        return Siligen::SharedKernel::Result<Siligen::Process::Recipes::ValueObjects::RecipeVersionId>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::RecipeVersion> GetVersionById(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id,
        const Siligen::Process::Recipes::ValueObjects::RecipeVersionId& version_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::RecipeVersion>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->GetVersionById(recipe_id, version_id);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::RecipeVersion>::Success(
                ToNewRecipeVersion(result.Value()));
        }
        return Siligen::SharedKernel::Result<Siligen::Process::Recipes::Aggregates::RecipeVersion>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::RecipeVersion>> ListVersions(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::RecipeVersion>>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Recipe repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->ListVersions(recipe_id);
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::RecipeVersion>>::Failure(
                ToNewError(result.GetError()));
        }
        std::vector<Siligen::Process::Recipes::Aggregates::RecipeVersion> converted;
        converted.reserve(result.Value().size());
        for (const auto& version : result.Value()) {
            converted.push_back(ToNewRecipeVersion(version));
        }
        return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::Aggregates::RecipeVersion>>::Success(
            std::move(converted));
    }

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> repository_;
};

class LegacyAuditRepositoryAdapter final : public Siligen::Process::Recipes::Ports::AuditRepositoryPort {
   public:
    explicit LegacyAuditRepositoryAdapter(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> repository) noexcept
        : repository_(std::move(repository)) {}

    Siligen::SharedKernel::VoidResult AppendRecord(
        const Siligen::Process::Recipes::ValueObjects::AuditRecord& record) override {
        if (!repository_) {
            return Siligen::SharedKernel::VoidResult::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Audit repository not available",
                    "RecipeUseCaseHelpers"));
        }
        return ToNewVoidResult(repository_->AppendRecord(ToLegacyAuditRecord(record)));
    }

    Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>> ListByRecipe(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Audit repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->ListByRecipe(recipe_id);
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Failure(
                ToNewError(result.GetError()));
        }
        std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord> converted;
        converted.reserve(result.Value().size());
        for (const auto& record : result.Value()) {
            converted.push_back(ToNewAuditRecord(record));
        }
        return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Success(
            std::move(converted));
    }

    Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>> ListByVersion(
        const Siligen::Process::Recipes::ValueObjects::RecipeId& recipe_id,
        const Siligen::Process::Recipes::ValueObjects::RecipeVersionId& version_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Failure(
                Siligen::SharedKernel::Error(
                    Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                    "Audit repository not available",
                    "RecipeUseCaseHelpers"));
        }
        const auto result = repository_->ListByVersion(recipe_id, version_id);
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Failure(
                ToNewError(result.GetError()));
        }
        std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord> converted;
        converted.reserve(result.Value().size());
        for (const auto& record : result.Value()) {
            converted.push_back(ToNewAuditRecord(record));
        }
        return Siligen::SharedKernel::Result<std::vector<Siligen::Process::Recipes::ValueObjects::AuditRecord>>::Success(
            std::move(converted));
    }

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> repository_;
};

inline Siligen::Shared::Types::Result<void> ValidateRecipeWithProcessCore(
    const Siligen::Domain::Recipes::Aggregates::Recipe& recipe) {
    Siligen::Process::Recipes::Services::RecipeValidationService service;
    return ToLegacyVoidResult(service.ValidateRecipe(ToNewRecipe(recipe)));
}

inline Siligen::Shared::Types::Result<void> ValidateRecipeVersionWithProcessCore(
    const Siligen::Domain::Recipes::Aggregates::RecipeVersion& version,
    const Siligen::Domain::Recipes::ValueObjects::ParameterSchema& schema) {
    Siligen::Process::Recipes::Services::RecipeValidationService service;
    return ToLegacyVoidResult(service.ValidateRecipeVersion(ToNewRecipeVersion(version), ToNewSchema(schema)));
}

inline Siligen::Shared::Types::Result<ActivateRecipeVersionResultData>
ActivateRecipeVersionWithProcessCore(
    const std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort>& recipe_repository,
    const std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort>& audit_repository,
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
    const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id,
    Siligen::Domain::Recipes::ValueObjects::AuditAction audit_action,
    const std::string& actor,
    std::int64_t timestamp,
    const Siligen::Domain::Recipes::ValueObjects::AuditRecordId& audit_id) {
    auto recipe_adapter = std::make_shared<LegacyRecipeRepositoryAdapter>(recipe_repository);
    std::shared_ptr<Siligen::Process::Recipes::Ports::AuditRepositoryPort> audit_adapter;
    if (audit_repository) {
        audit_adapter = std::make_shared<LegacyAuditRepositoryAdapter>(audit_repository);
    }

    Siligen::Process::Recipes::Services::RecipeActivationService service(
        std::move(recipe_adapter),
        std::move(audit_adapter));
    const auto result = service.ActivateVersion(
        recipe_id,
        version_id,
        static_cast<Siligen::Process::Recipes::ValueObjects::AuditAction>(audit_action),
        actor,
        timestamp,
        audit_id);
    if (result.IsError()) {
        return Siligen::Shared::Types::Result<ActivateRecipeVersionResultData>::Failure(
            ToLegacyError(result.GetError()));
    }

    ActivateRecipeVersionResultData converted;
    converted.recipe_id = result.Value().recipe_id;
    converted.active_version_id = result.Value().active_version_id;
    return Siligen::Shared::Types::Result<ActivateRecipeVersionResultData>::Success(std::move(converted));
}

}  // namespace Siligen::Application::UseCases::Recipes
