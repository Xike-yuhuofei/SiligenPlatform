#include "modules/device-hal/src/adapters/recipes/RecipeJsonSerializer.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Siligen::Infrastructure::Adapters::Recipes {
namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<std::string> RequireString(const RecipeJsonSerializer::json& j, const char* field) {
    if (!j.contains(field)) {
        return Result<std::string>::Failure(
            Error(ErrorCode::JSON_MISSING_REQUIRED_FIELD, std::string("Missing field: ") + field, "RecipeJsonSerializer"));
    }
    if (!j.at(field).is_string()) {
        return Result<std::string>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, std::string("Field is not string: ") + field, "RecipeJsonSerializer"));
    }
    return Result<std::string>::Success(j.at(field).get<std::string>());
}

Result<std::int64_t> RequireInt64(const RecipeJsonSerializer::json& j, const char* field) {
    if (!j.contains(field)) {
        return Result<std::int64_t>::Failure(
            Error(ErrorCode::JSON_MISSING_REQUIRED_FIELD, std::string("Missing field: ") + field, "RecipeJsonSerializer"));
    }
    if (!j.at(field).is_number_integer()) {
        return Result<std::int64_t>::Failure(
            Error(ErrorCode::JSON_INVALID_TYPE, std::string("Field is not int64: ") + field, "RecipeJsonSerializer"));
    }
    return Result<std::int64_t>::Success(j.at(field).get<std::int64_t>());
}

}  // namespace

Result<RecipeJsonSerializer::json> RecipeJsonSerializer::Parse(const std::string& payload) {
    try {
        return Result<json>::Success(json::parse(payload));
    } catch (const json::parse_error& e) {
        return Result<json>::Failure(
            Error(ErrorCode::JSON_PARSE_ERROR, std::string("JSON parse error: ") + e.what(), "RecipeJsonSerializer"));
    }
}

Result<std::string> RecipeJsonSerializer::Dump(const json& value) {
    try {
        return Result<std::string>::Success(value.dump(4));
    } catch (const json::type_error& e) {
        return Result<std::string>::Failure(
            Error(ErrorCode::SERIALIZATION_FAILED, std::string("JSON dump error: ") + e.what(), "RecipeJsonSerializer"));
    }
}

RecipeJsonSerializer::json RecipeJsonSerializer::RecipeToJson(
    const Siligen::Domain::Recipes::Aggregates::Recipe& recipe) {
    json j;
    j["id"] = recipe.id;
    j["name"] = recipe.name;
    j["description"] = recipe.description;
    j["status"] = RecipeStatusToString(recipe.status);
    j["tags"] = recipe.tags;
    if (!recipe.active_version_id.empty()) {
        j["activeVersionId"] = recipe.active_version_id;
    }
    j["versionIds"] = recipe.version_ids;
    j["createdAt"] = recipe.created_at;
    j["updatedAt"] = recipe.updated_at;
    return j;
}

Result<Siligen::Domain::Recipes::Aggregates::Recipe> RecipeJsonSerializer::RecipeFromJson(const json& j) {
    auto id_result = GetRequiredString(j, "id");
    if (id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::Recipe>::Failure(id_result.GetError());
    }
    auto name_result = GetRequiredString(j, "name");
    if (name_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::Recipe>::Failure(name_result.GetError());
    }

    Siligen::Domain::Recipes::Aggregates::Recipe recipe;
    recipe.id = id_result.Value();
    recipe.name = name_result.Value();
    recipe.description = j.value("description", "");

    auto status_result = RecipeStatusFromString(j.value("status", "active"));
    if (status_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::Recipe>::Failure(status_result.GetError());
    }
    recipe.status = status_result.Value();
    recipe.tags = j.value("tags", std::vector<std::string>{});
    recipe.active_version_id = j.value("activeVersionId", "");
    recipe.version_ids = j.value("versionIds", std::vector<std::string>{});
    recipe.created_at = j.value("createdAt", 0LL);
    recipe.updated_at = j.value("updatedAt", recipe.created_at);
    return Result<Siligen::Domain::Recipes::Aggregates::Recipe>::Success(recipe);
}

RecipeJsonSerializer::json RecipeJsonSerializer::RecipeVersionToJson(
    const Siligen::Domain::Recipes::Aggregates::RecipeVersion& version) {
    json j;
    j["id"] = version.id;
    j["recipeId"] = version.recipe_id;
    j["versionLabel"] = version.version_label;
    j["status"] = RecipeVersionStatusToString(version.status);
    if (!version.base_version_id.empty()) {
        j["baseVersionId"] = version.base_version_id;
    }
    json params = json::array();
    for (const auto& entry : version.parameters) {
        params.push_back(ParameterValueEntryToJson(entry));
    }
    j["parameters"] = params;
    j["createdBy"] = version.created_by;
    j["createdAt"] = version.created_at;
    if (version.published_at.has_value()) {
        j["publishedAt"] = version.published_at.value();
    }
    if (!version.change_note.empty()) {
        j["changeNote"] = version.change_note;
    }
    return j;
}

Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion> RecipeJsonSerializer::RecipeVersionFromJson(
    const json& j) {
    auto id_result = GetRequiredString(j, "id");
    if (id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Failure(id_result.GetError());
    }
    auto recipe_id_result = GetRequiredString(j, "recipeId");
    if (recipe_id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Failure(recipe_id_result.GetError());
    }
    auto label_result = GetRequiredString(j, "versionLabel");
    if (label_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Failure(label_result.GetError());
    }

    Siligen::Domain::Recipes::Aggregates::RecipeVersion version;
    version.id = id_result.Value();
    version.recipe_id = recipe_id_result.Value();
    version.version_label = label_result.Value();

    auto status_result = RecipeVersionStatusFromString(j.value("status", "draft"));
    if (status_result.IsError()) {
        return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Failure(status_result.GetError());
    }
    version.status = status_result.Value();
    version.base_version_id = j.value("baseVersionId", "");
    version.created_by = j.value("createdBy", "");
    version.created_at = j.value("createdAt", 0LL);
    if (j.contains("publishedAt") && j["publishedAt"].is_number_integer()) {
        version.published_at = j["publishedAt"].get<std::int64_t>();
    }
    version.change_note = j.value("changeNote", "");

    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& param : j["parameters"]) {
            auto entry_result = ParameterValueEntryFromJson(param);
            if (entry_result.IsError()) {
                return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Failure(entry_result.GetError());
            }
            version.parameters.push_back(entry_result.Value());
        }
    }

    return Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion>::Success(version);
}

RecipeJsonSerializer::json RecipeJsonSerializer::TemplateToJson(const Siligen::Domain::Recipes::ValueObjects::Template& data) {
    json j;
    j["id"] = data.id;
    j["name"] = data.name;
    j["description"] = data.description;
    json params = json::array();
    for (const auto& entry : data.parameters) {
        params.push_back(ParameterValueEntryToJson(entry));
    }
    j["parameters"] = params;
    j["createdAt"] = data.created_at;
    j["updatedAt"] = data.updated_at;
    return j;
}

Result<Siligen::Domain::Recipes::ValueObjects::Template> RecipeJsonSerializer::TemplateFromJson(const json& j) {
    auto id_result = GetRequiredString(j, "id");
    if (id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::Template>::Failure(id_result.GetError());
    }
    auto name_result = GetRequiredString(j, "name");
    if (name_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::Template>::Failure(name_result.GetError());
    }

    Siligen::Domain::Recipes::ValueObjects::Template data;
    data.id = id_result.Value();
    data.name = name_result.Value();
    data.description = j.value("description", "");
    data.created_at = j.value("createdAt", 0LL);
    data.updated_at = j.value("updatedAt", data.created_at);
    if (j.contains("parameters") && j["parameters"].is_array()) {
        for (const auto& param : j["parameters"]) {
            auto entry_result = ParameterValueEntryFromJson(param);
            if (entry_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::Template>::Failure(entry_result.GetError());
            }
            data.parameters.push_back(entry_result.Value());
        }
    }
    return Result<Siligen::Domain::Recipes::ValueObjects::Template>::Success(data);
}

RecipeJsonSerializer::json RecipeJsonSerializer::AuditRecordToJson(
    const Siligen::Domain::Recipes::ValueObjects::AuditRecord& record) {
    json j;
    j["id"] = record.id;
    j["recipeId"] = record.recipe_id;
    if (record.version_id.has_value()) {
        j["versionId"] = record.version_id.value();
    }
    j["action"] = AuditActionToString(record.action);
    j["actor"] = record.actor;
    j["timestamp"] = record.timestamp;
    json changes = json::array();
    for (const auto& change : record.changes) {
        changes.push_back(FieldChangeToJson(change));
    }
    j["changes"] = changes;
    return j;
}

Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord> RecipeJsonSerializer::AuditRecordFromJson(const json& j) {
    auto id_result = GetRequiredString(j, "id");
    if (id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(id_result.GetError());
    }
    auto recipe_id_result = GetRequiredString(j, "recipeId");
    if (recipe_id_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(recipe_id_result.GetError());
    }
    auto action_result = AuditActionFromString(j.value("action", ""));
    if (action_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(action_result.GetError());
    }
    auto actor_result = GetRequiredString(j, "actor");
    if (actor_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(actor_result.GetError());
    }
    auto timestamp_result = GetRequiredInt64(j, "timestamp");
    if (timestamp_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(timestamp_result.GetError());
    }

    Siligen::Domain::Recipes::ValueObjects::AuditRecord record;
    record.id = id_result.Value();
    record.recipe_id = recipe_id_result.Value();
    record.action = action_result.Value();
    record.actor = actor_result.Value();
    record.timestamp = timestamp_result.Value();
    if (j.contains("versionId") && j["versionId"].is_string()) {
        record.version_id = j["versionId"].get<std::string>();
    }
    if (j.contains("changes") && j["changes"].is_array()) {
        for (const auto& change : j["changes"]) {
            auto change_result = FieldChangeFromJson(change);
            if (change_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Failure(change_result.GetError());
            }
            record.changes.push_back(change_result.Value());
        }
    }
    return Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord>::Success(record);
}

RecipeJsonSerializer::json RecipeJsonSerializer::ParameterSchemaToJson(
    const Siligen::Domain::Recipes::ValueObjects::ParameterSchema& schema) {
    json j;
    j["schemaId"] = schema.schema_id;
    json entries = json::array();
    for (const auto& entry : schema.entries) {
        json e;
        e["key"] = entry.key;
        if (!entry.display_name.empty()) {
            e["displayName"] = entry.display_name;
        }
        e["type"] = ParameterValueTypeToString(entry.value_type);
        if (!entry.unit.empty()) {
            e["unit"] = entry.unit;
        }
        e["required"] = entry.required;
        json constraints;
        bool has_constraints = false;
        if (entry.constraints.min_value.has_value()) {
            constraints["min"] = entry.constraints.min_value.value();
            has_constraints = true;
        }
        if (entry.constraints.max_value.has_value()) {
            constraints["max"] = entry.constraints.max_value.value();
            has_constraints = true;
        }
        if (!entry.constraints.allowed_values.empty()) {
            constraints["allowedValues"] = entry.constraints.allowed_values;
            has_constraints = true;
        }
        if (has_constraints) {
            e["constraints"] = constraints;
        }
        entries.push_back(e);
    }
    j["entries"] = entries;
    return j;
}

Result<Siligen::Domain::Recipes::ValueObjects::ParameterSchema> RecipeJsonSerializer::ParameterSchemaFromJson(
    const json& j) {
    Siligen::Domain::Recipes::ValueObjects::ParameterSchema schema;
    schema.schema_id = j.value("schemaId", "");
    if (j.contains("entries") && j["entries"].is_array()) {
        for (const auto& entry_json : j["entries"]) {
            auto key_result = GetRequiredString(entry_json, "key");
            if (key_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::ParameterSchema>::Failure(key_result.GetError());
            }
            Siligen::Domain::Recipes::ValueObjects::ParameterSchemaEntry entry;
            entry.key = key_result.Value();
            entry.display_name = entry_json.value("displayName", entry.key);
            auto type_result = ParameterValueTypeFromString(entry_json.value("type", "string"));
            if (type_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::ParameterSchema>::Failure(type_result.GetError());
            }
            entry.value_type = type_result.Value();
            entry.unit = entry_json.value("unit", "");
            entry.required = entry_json.value("required", false);
            if (entry_json.contains("constraints") && entry_json["constraints"].is_object()) {
                const auto& constraints = entry_json["constraints"];
                if (constraints.contains("min") && constraints["min"].is_number()) {
                    entry.constraints.min_value = constraints["min"].get<double>();
                }
                if (constraints.contains("max") && constraints["max"].is_number()) {
                    entry.constraints.max_value = constraints["max"].get<double>();
                }
                if (constraints.contains("allowedValues") && constraints["allowedValues"].is_array()) {
                    entry.constraints.allowed_values = constraints["allowedValues"].get<std::vector<std::string>>();
                }
            }
            schema.entries.push_back(entry);
        }
    }
    return Result<Siligen::Domain::Recipes::ValueObjects::ParameterSchema>::Success(schema);
}

RecipeJsonSerializer::json RecipeJsonSerializer::FieldChangeToJson(
    const Siligen::Domain::Recipes::ValueObjects::FieldChange& change) {
    json j;
    j["field"] = change.field;
    if (!change.before.empty()) {
        j["before"] = change.before;
    }
    if (!change.after.empty()) {
        j["after"] = change.after;
    }
    return j;
}

Result<Siligen::Domain::Recipes::ValueObjects::FieldChange> RecipeJsonSerializer::FieldChangeFromJson(const json& j) {
    auto field_result = GetRequiredString(j, "field");
    if (field_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::FieldChange>::Failure(field_result.GetError());
    }
    Siligen::Domain::Recipes::ValueObjects::FieldChange change;
    change.field = field_result.Value();
    change.before = j.value("before", "");
    change.after = j.value("after", "");
    return Result<Siligen::Domain::Recipes::ValueObjects::FieldChange>::Success(change);
}

RecipeJsonSerializer::json RecipeJsonSerializer::ParameterValueEntryToJson(
    const Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry& entry) {
    json j;
    j["key"] = entry.key;
    j["value"] = ParameterValueToJson(entry.value);
    return j;
}

Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>
RecipeJsonSerializer::ParameterValueEntryFromJson(const json& j) {
    auto key_result = GetRequiredString(j, "key");
    if (key_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>::Failure(key_result.GetError());
    }
    if (!j.contains("value")) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>::Failure(
            Error(ErrorCode::JSON_MISSING_REQUIRED_FIELD, "Missing field: value", "RecipeJsonSerializer"));
    }
    auto value_result = ParameterValueFromJson(j["value"]);
    if (value_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>::Failure(value_result.GetError());
    }
    Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry entry;
    entry.key = key_result.Value();
    entry.value = value_result.Value();
    return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>::Success(entry);
}

RecipeJsonSerializer::json RecipeJsonSerializer::ParameterValueToJson(
    const Siligen::Domain::Recipes::ValueObjects::ParameterValue& value) {
    return std::visit(
        [](auto&& arg) -> json {
            return json(arg);
        },
        value);
}

Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue> RecipeJsonSerializer::ParameterValueFromJson(
    const json& j) {
    if (j.is_boolean()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue>::Success(j.get<bool>());
    }
    if (j.is_number_integer()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue>::Success(j.get<std::int64_t>());
    }
    if (j.is_number_float()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue>::Success(j.get<double>());
    }
    if (j.is_string()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue>::Success(j.get<std::string>());
    }
    return Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue>::Failure(
        Error(ErrorCode::JSON_INVALID_TYPE, "Unsupported parameter value type", "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::RecipeStatusToString(
    Siligen::Domain::Recipes::ValueObjects::RecipeStatus status) {
    using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
    switch (status) {
        case RecipeStatus::Active:
            return "active";
        case RecipeStatus::Archived:
            return "archived";
        default:
            return "active";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::RecipeStatus> RecipeJsonSerializer::RecipeStatusFromString(
    const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
    auto normalized = ToLowerCopy(value);
    if (normalized == "active") {
        return Result<RecipeStatus>::Success(RecipeStatus::Active);
    }
    if (normalized == "archived") {
        return Result<RecipeStatus>::Success(RecipeStatus::Archived);
    }
    return Result<RecipeStatus>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid recipe status: " + value, "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::RecipeVersionStatusToString(
    Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus status) {
    using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
    switch (status) {
        case RecipeVersionStatus::Draft:
            return "draft";
        case RecipeVersionStatus::Published:
            return "published";
        case RecipeVersionStatus::Archived:
            return "archived";
        default:
            return "draft";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus>
RecipeJsonSerializer::RecipeVersionStatusFromString(const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
    auto normalized = ToLowerCopy(value);
    if (normalized == "draft") {
        return Result<RecipeVersionStatus>::Success(RecipeVersionStatus::Draft);
    }
    if (normalized == "published") {
        return Result<RecipeVersionStatus>::Success(RecipeVersionStatus::Published);
    }
    if (normalized == "archived") {
        return Result<RecipeVersionStatus>::Success(RecipeVersionStatus::Archived);
    }
    return Result<RecipeVersionStatus>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid version status: " + value, "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::ParameterValueTypeToString(
    Siligen::Domain::Recipes::ValueObjects::ParameterValueType type) {
    using Siligen::Domain::Recipes::ValueObjects::ParameterValueType;
    switch (type) {
        case ParameterValueType::Integer:
            return "int";
        case ParameterValueType::Float:
            return "float";
        case ParameterValueType::Boolean:
            return "bool";
        case ParameterValueType::String:
            return "string";
        case ParameterValueType::Enum:
            return "enum";
        default:
            return "string";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueType>
RecipeJsonSerializer::ParameterValueTypeFromString(const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::ParameterValueType;
    auto normalized = ToLowerCopy(value);
    if (normalized == "int" || normalized == "integer") {
        return Result<ParameterValueType>::Success(ParameterValueType::Integer);
    }
    if (normalized == "float" || normalized == "double") {
        return Result<ParameterValueType>::Success(ParameterValueType::Float);
    }
    if (normalized == "bool" || normalized == "boolean") {
        return Result<ParameterValueType>::Success(ParameterValueType::Boolean);
    }
    if (normalized == "string") {
        return Result<ParameterValueType>::Success(ParameterValueType::String);
    }
    if (normalized == "enum") {
        return Result<ParameterValueType>::Success(ParameterValueType::Enum);
    }
    return Result<ParameterValueType>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid parameter type: " + value, "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::AuditActionToString(
    Siligen::Domain::Recipes::ValueObjects::AuditAction action) {
    using Siligen::Domain::Recipes::ValueObjects::AuditAction;
    switch (action) {
        case AuditAction::Create:
            return "create";
        case AuditAction::Edit:
            return "edit";
        case AuditAction::Publish:
            return "publish";
        case AuditAction::Rollback:
            return "rollback";
        case AuditAction::Archive:
            return "archive";
        case AuditAction::Import:
            return "import";
        case AuditAction::Export:
            return "export";
        default:
            return "create";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::AuditAction> RecipeJsonSerializer::AuditActionFromString(
    const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::AuditAction;
    auto normalized = ToLowerCopy(value);
    if (normalized == "create") {
        return Result<AuditAction>::Success(AuditAction::Create);
    }
    if (normalized == "edit") {
        return Result<AuditAction>::Success(AuditAction::Edit);
    }
    if (normalized == "publish") {
        return Result<AuditAction>::Success(AuditAction::Publish);
    }
    if (normalized == "rollback") {
        return Result<AuditAction>::Success(AuditAction::Rollback);
    }
    if (normalized == "archive") {
        return Result<AuditAction>::Success(AuditAction::Archive);
    }
    if (normalized == "import") {
        return Result<AuditAction>::Success(AuditAction::Import);
    }
    if (normalized == "export") {
        return Result<AuditAction>::Success(AuditAction::Export);
    }
    return Result<AuditAction>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid audit action: " + value, "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::ImportConflictTypeToString(
    Siligen::Domain::Recipes::ValueObjects::ImportConflictType type) {
    using Siligen::Domain::Recipes::ValueObjects::ImportConflictType;
    switch (type) {
        case ImportConflictType::Name:
            return "name";
        case ImportConflictType::Version:
            return "version";
        default:
            return "name";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::ImportConflictType> RecipeJsonSerializer::ImportConflictTypeFromString(
    const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::ImportConflictType;
    auto normalized = ToLowerCopy(value);
    if (normalized == "name") {
        return Result<ImportConflictType>::Success(ImportConflictType::Name);
    }
    if (normalized == "version") {
        return Result<ImportConflictType>::Success(ImportConflictType::Version);
    }
    return Result<ImportConflictType>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid conflict type: " + value, "RecipeJsonSerializer"));
}

std::string RecipeJsonSerializer::ConflictResolutionToString(
    Siligen::Domain::Recipes::ValueObjects::ConflictResolution resolution) {
    using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
    switch (resolution) {
        case ConflictResolution::Rename:
            return "rename";
        case ConflictResolution::Skip:
            return "skip";
        case ConflictResolution::Replace:
            return "replace";
        default:
            return "rename";
    }
}

Result<Siligen::Domain::Recipes::ValueObjects::ConflictResolution>
RecipeJsonSerializer::ConflictResolutionFromString(const std::string& value) {
    using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
    auto normalized = ToLowerCopy(value);
    if (normalized == "rename") {
        return Result<ConflictResolution>::Success(ConflictResolution::Rename);
    }
    if (normalized == "skip") {
        return Result<ConflictResolution>::Success(ConflictResolution::Skip);
    }
    if (normalized == "replace") {
        return Result<ConflictResolution>::Success(ConflictResolution::Replace);
    }
    return Result<ConflictResolution>::Failure(
        Error(ErrorCode::ENUM_CONVERSION_FAILED, "Invalid conflict resolution: " + value, "RecipeJsonSerializer"));
}

Result<std::string> RecipeJsonSerializer::GetRequiredString(const json& j, const char* field) {
    return RequireString(j, field);
}

Result<std::int64_t> RecipeJsonSerializer::GetRequiredInt64(const json& j, const char* field) {
    return RequireInt64(j, field);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
