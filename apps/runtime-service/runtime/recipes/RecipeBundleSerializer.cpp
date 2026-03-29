#include "RecipeBundleSerializer.h"

#include "shared/types/SerializationTypes.h"

namespace {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::CreateMissingFieldError;
using Siligen::Shared::Types::CreateSerializationError;
using Siligen::Shared::Types::ErrorCode;

Result<std::string> ReadString(const nlohmann::json& value, const std::string& field) {
    if (!value.contains(field)) {
        return Result<std::string>::Failure(CreateMissingFieldError(field));
    }
    try {
        return Result<std::string>::Success(value.at(field).get<std::string>());
    } catch (const nlohmann::json::type_error&) {
        return Result<std::string>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + field + " is not a string"));
    }
}

Result<std::int64_t> ReadInt64(const nlohmann::json& value, const std::string& field) {
    if (!value.contains(field)) {
        return Result<std::int64_t>::Failure(CreateMissingFieldError(field));
    }
    try {
        return Result<std::int64_t>::Success(value.at(field).get<std::int64_t>());
    } catch (const nlohmann::json::type_error&) {
        return Result<std::int64_t>::Failure(
            CreateSerializationError(ErrorCode::JSON_INVALID_TYPE,
                                     "Field " + field + " is not an integer"));
    }
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Recipes {

using Siligen::Shared::Types::Result;
using RecipeBundle = Siligen::Domain::Recipes::ValueObjects::RecipeBundle;

Result<std::string> RecipeBundleSerializer::Serialize(const RecipeBundle& bundle) {
    auto json_value = BundleToJson(bundle);
    return RecipeJsonSerializer::Dump(json_value);
}

Result<RecipeBundle> RecipeBundleSerializer::Deserialize(const std::string& payload) {
    auto json_result = RecipeJsonSerializer::Parse(payload);
    if (json_result.IsError()) {
        return Result<RecipeBundle>::Failure(json_result.GetError());
    }

    return BundleFromJson(json_result.Value());
}

RecipeBundleSerializer::json RecipeBundleSerializer::BundleToJson(const RecipeBundle& bundle) {
    json output;
    output["schemaVersion"] = bundle.schema_version.empty() ? "1.0" : bundle.schema_version;
    output["exportedAt"] = bundle.exported_at;

    json recipes = json::array();
    for (const auto& recipe : bundle.recipes) {
        recipes.push_back(RecipeJsonSerializer::RecipeToJson(recipe));
    }
    output["recipes"] = recipes;

    json versions = json::array();
    for (const auto& version : bundle.versions) {
        versions.push_back(RecipeJsonSerializer::RecipeVersionToJson(version));
    }
    output["versions"] = versions;

    json templates = json::array();
    for (const auto& template_data : bundle.templates) {
        templates.push_back(RecipeJsonSerializer::TemplateToJson(template_data));
    }
    output["templates"] = templates;

    json audits = json::array();
    for (const auto& record : bundle.audit_records) {
        audits.push_back(RecipeJsonSerializer::AuditRecordToJson(record));
    }
    output["auditRecords"] = audits;

    return output;
}

Result<RecipeBundle> RecipeBundleSerializer::BundleFromJson(const json& value) {
    RecipeBundle bundle;

    auto schema = ReadString(value, "schemaVersion");
    if (schema.IsError()) {
        return Result<RecipeBundle>::Failure(schema.GetError());
    }
    bundle.schema_version = schema.Value();

    if (value.contains("exportedAt")) {
        auto exported = ReadInt64(value, "exportedAt");
        if (exported.IsError()) {
            return Result<RecipeBundle>::Failure(exported.GetError());
        }
        bundle.exported_at = exported.Value();
    }

    if (value.contains("recipes") && value.at("recipes").is_array()) {
        for (const auto& item : value.at("recipes")) {
            auto recipe_result = RecipeJsonSerializer::RecipeFromJson(item);
            if (recipe_result.IsError()) {
                return Result<RecipeBundle>::Failure(recipe_result.GetError());
            }
            bundle.recipes.push_back(recipe_result.Value());
        }
    }

    if (value.contains("versions") && value.at("versions").is_array()) {
        for (const auto& item : value.at("versions")) {
            auto version_result = RecipeJsonSerializer::RecipeVersionFromJson(item);
            if (version_result.IsError()) {
                return Result<RecipeBundle>::Failure(version_result.GetError());
            }
            bundle.versions.push_back(version_result.Value());
        }
    }

    if (value.contains("templates") && value.at("templates").is_array()) {
        for (const auto& item : value.at("templates")) {
            auto template_result = RecipeJsonSerializer::TemplateFromJson(item);
            if (template_result.IsError()) {
                return Result<RecipeBundle>::Failure(template_result.GetError());
            }
            bundle.templates.push_back(template_result.Value());
        }
    }

    if (value.contains("auditRecords") && value.at("auditRecords").is_array()) {
        for (const auto& item : value.at("auditRecords")) {
            auto record_result = RecipeJsonSerializer::AuditRecordFromJson(item);
            if (record_result.IsError()) {
                return Result<RecipeBundle>::Failure(record_result.GetError());
            }
            bundle.audit_records.push_back(record_result.Value());
        }
    }

    return Result<RecipeBundle>::Success(bundle);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
