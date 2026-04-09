#include "recipe_lifecycle/application/usecases/recipes/ImportRecipeBundlePayloadUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <unordered_set>

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Domain::Recipes::ValueObjects::AuditAction;
using Siligen::Domain::Recipes::ValueObjects::AuditRecord;
using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
using Siligen::Domain::Recipes::ValueObjects::ImportConflict;
using Siligen::Domain::Recipes::ValueObjects::ImportConflictType;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

ConflictResolution ParseResolution(const std::string& value) {
    auto normalized = ToLowerCopy(value);
    if (normalized == "skip") {
        return ConflictResolution::Skip;
    }
    if (normalized == "replace") {
        return ConflictResolution::Replace;
    }
    return ConflictResolution::Rename;
}

ImportConflict BuildNameConflict(const std::string& incoming_name,
                                 const std::string& existing_id,
                                 ConflictResolution suggestion) {
    ImportConflict conflict;
    conflict.type = ImportConflictType::Name;
    conflict.message = "Recipe name conflict: " + incoming_name;
    conflict.existing_id = existing_id;
    conflict.incoming_name = incoming_name;
    conflict.suggested_resolution = suggestion;
    return conflict;
}

ImportConflict BuildVersionConflict(const std::string& version_id, ConflictResolution suggestion) {
    ImportConflict conflict;
    conflict.type = ImportConflictType::Version;
    conflict.message = "Recipe version conflict: " + version_id;
    conflict.existing_id = version_id;
    conflict.incoming_name = "";
    conflict.suggested_resolution = suggestion;
    return conflict;
}

std::string ResolveSourceLabel(const std::string& source_label) {
    return source_label.empty() ? "<inline>" : source_label;
}

}  // namespace

ImportRecipeBundlePayloadUseCase::ImportRecipeBundlePayloadUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port)
    : recipe_repository_(std::move(recipe_repository)),
      template_repository_(std::move(template_repository)),
      audit_repository_(std::move(audit_repository)),
      serializer_port_(std::move(serializer_port)) {}

Result<ImportRecipeBundlePayloadResponse> ImportRecipeBundlePayloadUseCase::Execute(
    const ImportRecipeBundlePayloadRequest& request) {
    if (!recipe_repository_ || !serializer_port_) {
        return Result<ImportRecipeBundlePayloadResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED,
                  "Recipe dependencies not available",
                  "ImportRecipeBundlePayloadUseCase"));
    }
    if (request.bundle_json.empty()) {
        return Result<ImportRecipeBundlePayloadResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "Bundle payload is required",
                  "ImportRecipeBundlePayloadUseCase"));
    }

    auto bundle_result = serializer_port_->Deserialize(request.bundle_json);
    if (bundle_result.IsError()) {
        return Result<ImportRecipeBundlePayloadResponse>::Failure(bundle_result.GetError());
    }

    auto bundle = bundle_result.Value();
    auto resolution = ParseResolution(request.resolution_strategy);

    std::unordered_map<std::string, std::string> name_to_id;
    std::unordered_set<std::string> existing_names;
    std::unordered_set<std::string> existing_ids;
    auto existing_recipes_result = recipe_repository_->ListRecipes({});
    if (existing_recipes_result.IsSuccess()) {
        for (const auto& recipe : existing_recipes_result.Value()) {
            name_to_id[ToLowerCopy(recipe.name)] = recipe.id;
            existing_names.insert(ToLowerCopy(recipe.name));
            existing_ids.insert(recipe.id);
        }
    }

    std::vector<ImportConflict> conflicts;
    std::unordered_map<std::string, std::string> recipe_id_map;
    std::unordered_set<std::string> skipped_recipe_ids;

    for (auto& recipe : bundle.recipes) {
        const auto name_key = ToLowerCopy(recipe.name);
        const bool name_conflict = existing_names.find(name_key) != existing_names.end();
        const bool id_conflict = existing_ids.find(recipe.id) != existing_ids.end();

        if (name_conflict || id_conflict) {
            if (name_conflict) {
                conflicts.push_back(BuildNameConflict(recipe.name, name_to_id[name_key], resolution));
            }
            if (id_conflict) {
                conflicts.push_back(BuildVersionConflict(recipe.id, resolution));
            }
        }
    }

    if (!conflicts.empty() && (request.dry_run || request.resolution_strategy.empty())) {
        ImportRecipeBundlePayloadResponse response;
        response.status = "conflicts";
        response.conflicts = conflicts;
        return Result<ImportRecipeBundlePayloadResponse>::Success(response);
    }

    for (auto& recipe : bundle.recipes) {
        const auto name_key = ToLowerCopy(recipe.name);
        const bool name_conflict = existing_names.find(name_key) != existing_names.end();
        const bool id_conflict = existing_ids.find(recipe.id) != existing_ids.end();

        if (name_conflict || id_conflict) {
            if (resolution == ConflictResolution::Skip) {
                skipped_recipe_ids.insert(recipe.id);
                continue;
            }
            if (resolution == ConflictResolution::Rename) {
                if (name_conflict) {
                    int suffix = 1;
                    std::string new_name = recipe.name + "_imported";
                    while (existing_names.find(ToLowerCopy(new_name)) != existing_names.end()) {
                        new_name = recipe.name + "_imported_" + std::to_string(suffix++);
                    }
                    recipe.name = new_name;
                    existing_names.insert(ToLowerCopy(new_name));
                }
                if (id_conflict) {
                    const std::string new_id = GenerateId("recipe");
                    recipe_id_map[recipe.id] = new_id;
                    recipe.id = new_id;
                    existing_ids.insert(new_id);
                }
            } else if (resolution == ConflictResolution::Replace) {
                if (name_conflict && name_to_id.count(name_key)) {
                    recipe_id_map[recipe.id] = name_to_id[name_key];
                    recipe.id = name_to_id[name_key];
                }
            }
        }
        existing_names.insert(ToLowerCopy(recipe.name));
        existing_ids.insert(recipe.id);
    }

    for (auto& version : bundle.versions) {
        if (recipe_id_map.count(version.recipe_id)) {
            version.recipe_id = recipe_id_map[version.recipe_id];
        }
    }
    for (auto& record : bundle.audit_records) {
        if (recipe_id_map.count(record.recipe_id)) {
            record.recipe_id = recipe_id_map[record.recipe_id];
        }
    }

    size_t imported_count = 0;
    if (!request.dry_run) {
        for (const auto& recipe : bundle.recipes) {
            if (skipped_recipe_ids.find(recipe.id) != skipped_recipe_ids.end()) {
                continue;
            }
            auto save_result = recipe_repository_->SaveRecipe(recipe);
            if (save_result.IsError()) {
                return Result<ImportRecipeBundlePayloadResponse>::Failure(save_result.GetError());
            }
            ++imported_count;
        }

        for (const auto& version : bundle.versions) {
            if (skipped_recipe_ids.find(version.recipe_id) != skipped_recipe_ids.end()) {
                continue;
            }
            auto save_result = recipe_repository_->SaveVersion(version);
            if (save_result.IsError()) {
                return Result<ImportRecipeBundlePayloadResponse>::Failure(save_result.GetError());
            }
        }

        if (template_repository_) {
            for (const auto& tmpl : bundle.templates) {
                template_repository_->SaveTemplate(tmpl);
            }
        }

        if (audit_repository_) {
            for (const auto& record : bundle.audit_records) {
                if (skipped_recipe_ids.find(record.recipe_id) != skipped_recipe_ids.end()) {
                    continue;
                }
                audit_repository_->AppendRecord(record);
            }

            const std::string source_label = ResolveSourceLabel(request.source_label);
            for (const auto& recipe : bundle.recipes) {
                if (skipped_recipe_ids.find(recipe.id) != skipped_recipe_ids.end()) {
                    continue;
                }
                AuditRecord record;
                record.id = GenerateId("audit");
                record.recipe_id = recipe.id;
                record.action = AuditAction::Import;
                record.actor = request.actor.empty() ? "system" : request.actor;
                record.timestamp = NowEpochMillis();
                record.changes = DiffStrings("import", "", source_label);
                audit_repository_->AppendRecord(record);
            }
        }
    }

    ImportRecipeBundlePayloadResponse response;
    response.status = request.dry_run ? "validated" : "imported";
    response.conflicts = conflicts;
    response.imported_count = imported_count;
    return Result<ImportRecipeBundlePayloadResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes
