#include "domain/recipes/domain-services/RecipeActivationService.h"

#include "siligen/process/recipes/services/recipe_activation_service.h"

#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Domain::Recipes::DomainServices {
namespace {

using LegacyAuditRepositoryPort = Siligen::Domain::Recipes::Ports::IAuditRepositoryPort;
using LegacyRecipeRepositoryPort = Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
namespace NewAggregates = Siligen::Process::Recipes::Aggregates;
namespace NewPorts = Siligen::Process::Recipes::Ports;
namespace NewServices = Siligen::Process::Recipes::Services;
namespace NewValueObjects = Siligen::Process::Recipes::ValueObjects;

constexpr const char* kModuleName = "RecipeActivationService";

Error ToLegacyError(const Siligen::SharedKernel::Error& error) {
    return Error(static_cast<ErrorCode>(static_cast<int>(error.GetCode())), error.GetMessage(), error.GetModule());
}

Siligen::SharedKernel::Error ToNewError(const Error& error) {
    return Siligen::SharedKernel::Error(static_cast<Siligen::SharedKernel::ErrorCode>(static_cast<int>(error.GetCode())),
                                        error.GetMessage(),
                                        error.GetModule());
}

Siligen::SharedKernel::VoidResult ToNewVoidResult(const Result<void>& result) {
    if (result.IsSuccess()) {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    return Siligen::SharedKernel::VoidResult::Failure(ToNewError(result.GetError()));
}

ValueObjects::RecipeListFilter ToLegacyRecipeListFilter(const NewValueObjects::RecipeListFilter& filter) {
    ValueObjects::RecipeListFilter converted;
    if (filter.status.has_value()) {
        converted.status =
            static_cast<ValueObjects::RecipeStatus>(filter.status.value());
    }
    converted.query = filter.query;
    converted.tag = filter.tag;
    return converted;
}

NewValueObjects::ParameterValueEntry ToNewParameterValueEntry(const ValueObjects::ParameterValueEntry& entry) {
    return NewValueObjects::ParameterValueEntry{entry.key, entry.value};
}

ValueObjects::ParameterValueEntry ToLegacyParameterValueEntry(const NewValueObjects::ParameterValueEntry& entry) {
    return ValueObjects::ParameterValueEntry{entry.key, entry.value};
}

NewValueObjects::FieldChange ToNewFieldChange(const ValueObjects::FieldChange& change) {
    return NewValueObjects::FieldChange{change.field, change.before, change.after};
}

ValueObjects::FieldChange ToLegacyFieldChange(const NewValueObjects::FieldChange& change) {
    return ValueObjects::FieldChange{change.field, change.before, change.after};
}

NewValueObjects::AuditRecord ToNewAuditRecord(const AuditRecord& record) {
    NewValueObjects::AuditRecord converted;
    converted.id = record.id;
    converted.recipe_id = record.recipe_id;
    converted.version_id = record.version_id;
    converted.action = static_cast<NewValueObjects::AuditAction>(record.action);
    converted.actor = record.actor;
    converted.timestamp = record.timestamp;
    converted.changes.reserve(record.changes.size());
    for (const auto& change : record.changes) {
        converted.changes.push_back(ToNewFieldChange(change));
    }
    return converted;
}

AuditRecord ToLegacyAuditRecord(const NewValueObjects::AuditRecord& record) {
    AuditRecord converted;
    converted.id = record.id;
    converted.recipe_id = record.recipe_id;
    converted.version_id = record.version_id;
    converted.action = static_cast<AuditAction>(record.action);
    converted.actor = record.actor;
    converted.timestamp = record.timestamp;
    converted.changes.reserve(record.changes.size());
    for (const auto& change : record.changes) {
        converted.changes.push_back(ToLegacyFieldChange(change));
    }
    return converted;
}

NewAggregates::Recipe ToNewRecipe(const Aggregates::Recipe& recipe) {
    NewAggregates::Recipe converted;
    converted.id = recipe.id;
    converted.name = recipe.name;
    converted.description = recipe.description;
    converted.status = static_cast<NewValueObjects::RecipeStatus>(recipe.status);
    converted.tags = recipe.tags;
    converted.active_version_id = recipe.active_version_id;
    converted.version_ids = recipe.version_ids;
    converted.created_at = recipe.created_at;
    converted.updated_at = recipe.updated_at;
    return converted;
}

Aggregates::Recipe ToLegacyRecipe(const NewAggregates::Recipe& recipe) {
    Aggregates::Recipe converted;
    converted.id = recipe.id;
    converted.name = recipe.name;
    converted.description = recipe.description;
    converted.status = static_cast<RecipeStatus>(recipe.status);
    converted.tags = recipe.tags;
    converted.active_version_id = recipe.active_version_id;
    converted.version_ids = recipe.version_ids;
    converted.created_at = recipe.created_at;
    converted.updated_at = recipe.updated_at;
    return converted;
}

NewAggregates::RecipeVersion ToNewRecipeVersion(const Aggregates::RecipeVersion& version) {
    NewAggregates::RecipeVersion converted;
    converted.id = version.id;
    converted.recipe_id = version.recipe_id;
    converted.version_label = version.version_label;
    converted.status = static_cast<NewValueObjects::RecipeVersionStatus>(version.status);
    converted.base_version_id = version.base_version_id;
    converted.parameters.reserve(version.parameters.size());
    for (const auto& parameter : version.parameters) {
        converted.parameters.push_back(ToNewParameterValueEntry(parameter));
    }
    converted.created_by = version.created_by;
    converted.created_at = version.created_at;
    converted.published_at = version.published_at;
    converted.change_note = version.change_note;
    return converted;
}

Aggregates::RecipeVersion ToLegacyRecipeVersion(const NewAggregates::RecipeVersion& version) {
    Aggregates::RecipeVersion converted;
    converted.id = version.id;
    converted.recipe_id = version.recipe_id;
    converted.version_label = version.version_label;
    converted.status = static_cast<RecipeVersionStatus>(version.status);
    converted.base_version_id = version.base_version_id;
    converted.parameters.reserve(version.parameters.size());
    for (const auto& parameter : version.parameters) {
        converted.parameters.push_back(ToLegacyParameterValueEntry(parameter));
    }
    converted.created_by = version.created_by;
    converted.created_at = version.created_at;
    converted.published_at = version.published_at;
    converted.change_note = version.change_note;
    return converted;
}

std::vector<NewAggregates::RecipeVersion> ToNewRecipeVersions(const std::vector<Aggregates::RecipeVersion>& versions) {
    std::vector<NewAggregates::RecipeVersion> converted;
    converted.reserve(versions.size());
    for (const auto& version : versions) {
        converted.push_back(ToNewRecipeVersion(version));
    }
    return converted;
}

class LegacyRecipeRepositoryAdapter final : public NewPorts::RecipeRepositoryPort {
   public:
    explicit LegacyRecipeRepositoryAdapter(std::shared_ptr<LegacyRecipeRepositoryPort> repository) noexcept
        : repository_(std::move(repository)) {}

    Siligen::SharedKernel::Result<NewValueObjects::RecipeId> SaveRecipe(const NewAggregates::Recipe& recipe) override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<NewValueObjects::RecipeId>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->SaveRecipe(ToLegacyRecipe(recipe));
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<NewValueObjects::RecipeId>::Success(result.Value());
        }
        return Siligen::SharedKernel::Result<NewValueObjects::RecipeId>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<NewAggregates::Recipe> GetRecipeById(
        const NewValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->GetRecipeById(recipe_id);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Success(ToNewRecipe(result.Value()));
        }
        return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<NewAggregates::Recipe> GetRecipeByName(const std::string& name) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->GetRecipeByName(name);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Success(ToNewRecipe(result.Value()));
        }
        return Siligen::SharedKernel::Result<NewAggregates::Recipe>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<std::vector<NewAggregates::Recipe>> ListRecipes(
        const NewValueObjects::RecipeListFilter& filter) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<NewAggregates::Recipe>>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->ListRecipes(ToLegacyRecipeListFilter(filter));
        if (result.IsSuccess()) {
            std::vector<NewAggregates::Recipe> converted;
            converted.reserve(result.Value().size());
            for (const auto& recipe : result.Value()) {
                converted.push_back(ToNewRecipe(recipe));
            }
            return Siligen::SharedKernel::Result<std::vector<NewAggregates::Recipe>>::Success(std::move(converted));
        }
        return Siligen::SharedKernel::Result<std::vector<NewAggregates::Recipe>>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::VoidResult ArchiveRecipe(const NewValueObjects::RecipeId& recipe_id) override {
        if (!repository_) {
            return Siligen::SharedKernel::VoidResult::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        return ToNewVoidResult(repository_->ArchiveRecipe(recipe_id));
    }

    Siligen::SharedKernel::Result<NewValueObjects::RecipeVersionId> SaveVersion(
        const NewAggregates::RecipeVersion& version) override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<NewValueObjects::RecipeVersionId>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->SaveVersion(ToLegacyRecipeVersion(version));
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<NewValueObjects::RecipeVersionId>::Success(result.Value());
        }
        return Siligen::SharedKernel::Result<NewValueObjects::RecipeVersionId>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<NewAggregates::RecipeVersion> GetVersionById(
        const NewValueObjects::RecipeId& recipe_id,
        const NewValueObjects::RecipeVersionId& version_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<NewAggregates::RecipeVersion>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->GetVersionById(recipe_id, version_id);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<NewAggregates::RecipeVersion>::Success(
                ToNewRecipeVersion(result.Value()));
        }
        return Siligen::SharedKernel::Result<NewAggregates::RecipeVersion>::Failure(ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<std::vector<NewAggregates::RecipeVersion>> ListVersions(
        const NewValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<NewAggregates::RecipeVersion>>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Recipe repository not available",
                                             kModuleName));
        }
        const auto result = repository_->ListVersions(recipe_id);
        if (result.IsSuccess()) {
            return Siligen::SharedKernel::Result<std::vector<NewAggregates::RecipeVersion>>::Success(
                ToNewRecipeVersions(result.Value()));
        }
        return Siligen::SharedKernel::Result<std::vector<NewAggregates::RecipeVersion>>::Failure(
            ToNewError(result.GetError()));
    }

   private:
    std::shared_ptr<LegacyRecipeRepositoryPort> repository_;
};

class LegacyAuditRepositoryAdapter final : public NewPorts::AuditRepositoryPort {
   public:
    explicit LegacyAuditRepositoryAdapter(std::shared_ptr<LegacyAuditRepositoryPort> repository) noexcept
        : repository_(std::move(repository)) {}

    Siligen::SharedKernel::VoidResult AppendRecord(const NewValueObjects::AuditRecord& record) override {
        if (!repository_) {
            return Siligen::SharedKernel::VoidResult::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Audit repository not available",
                                             kModuleName));
        }
        return ToNewVoidResult(repository_->AppendRecord(ToLegacyAuditRecord(record)));
    }

    Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>> ListByRecipe(
        const NewValueObjects::RecipeId& recipe_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Audit repository not available",
                                             kModuleName));
        }
        const auto result = repository_->ListByRecipe(recipe_id);
        if (result.IsSuccess()) {
            std::vector<NewValueObjects::AuditRecord> converted;
            converted.reserve(result.Value().size());
            for (const auto& record : result.Value()) {
                converted.push_back(ToNewAuditRecord(record));
            }
            return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Success(
                std::move(converted));
        }
        return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Failure(
            ToNewError(result.GetError()));
    }

    Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>> ListByVersion(
        const NewValueObjects::RecipeId& recipe_id,
        const NewValueObjects::RecipeVersionId& version_id) const override {
        if (!repository_) {
            return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Failure(
                Siligen::SharedKernel::Error(Siligen::SharedKernel::ErrorCode::PORT_NOT_INITIALIZED,
                                             "Audit repository not available",
                                             kModuleName));
        }
        const auto result = repository_->ListByVersion(recipe_id, version_id);
        if (result.IsSuccess()) {
            std::vector<NewValueObjects::AuditRecord> converted;
            converted.reserve(result.Value().size());
            for (const auto& record : result.Value()) {
                converted.push_back(ToNewAuditRecord(record));
            }
            return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Success(
                std::move(converted));
        }
        return Siligen::SharedKernel::Result<std::vector<NewValueObjects::AuditRecord>>::Failure(
            ToNewError(result.GetError()));
    }

   private:
    std::shared_ptr<LegacyAuditRepositoryPort> repository_;
};

}  // namespace

RecipeActivationService::RecipeActivationService(
    std::shared_ptr<Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Ports::IAuditRepositoryPort> audit_repository) noexcept
    : recipe_repository_(std::move(recipe_repository)),
      audit_repository_(std::move(audit_repository)) {}

Result<ActivateRecipeVersionResult> RecipeActivationService::ActivateVersion(
    const RecipeId& recipe_id,
    const RecipeVersionId& version_id,
    AuditAction audit_action,
    const std::string& actor,
    std::int64_t timestamp,
    const AuditRecordId& audit_id) noexcept {
    auto recipe_repository = std::make_shared<LegacyRecipeRepositoryAdapter>(recipe_repository_);
    std::shared_ptr<NewPorts::AuditRepositoryPort> audit_repository;
    if (audit_repository_) {
        audit_repository = std::make_shared<LegacyAuditRepositoryAdapter>(audit_repository_);
    }

    NewServices::RecipeActivationService service(std::move(recipe_repository), std::move(audit_repository));
    const auto result = service.ActivateVersion(recipe_id,
                                                version_id,
                                                static_cast<NewValueObjects::AuditAction>(audit_action),
                                                actor,
                                                timestamp,
                                                audit_id);
    if (result.IsError()) {
        return Result<ActivateRecipeVersionResult>::Failure(ToLegacyError(result.GetError()));
    }

    ActivateRecipeVersionResult converted;
    converted.recipe_id = result.Value().recipe_id;
    converted.active_version_id = result.Value().active_version_id;
    return Result<ActivateRecipeVersionResult>::Success(std::move(converted));
}

}  // namespace Siligen::Domain::Recipes::DomainServices
