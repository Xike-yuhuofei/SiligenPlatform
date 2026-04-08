#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/value-objects/ParameterSchema.h"
#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include "nlohmann/json.hpp"

#include <cstdint>
#include <string>

namespace Siligen::Domain::Recipes::Serialization {

class RecipeJsonSerializer {
   public:
    using json = nlohmann::json;

    static Siligen::Shared::Types::Result<json> Parse(const std::string& payload);
    static Siligen::Shared::Types::Result<std::string> Dump(const json& value);

    static json RecipeToJson(const Siligen::Domain::Recipes::Aggregates::Recipe& recipe);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::Aggregates::Recipe> RecipeFromJson(const json& j);

    static json RecipeVersionToJson(const Siligen::Domain::Recipes::Aggregates::RecipeVersion& version);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::Aggregates::RecipeVersion> RecipeVersionFromJson(
        const json& j);

    static json TemplateToJson(const Siligen::Domain::Recipes::ValueObjects::Template& data);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::Template> TemplateFromJson(
        const json& j);

    static json AuditRecordToJson(const Siligen::Domain::Recipes::ValueObjects::AuditRecord& record);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::AuditRecord> AuditRecordFromJson(
        const json& j);

    static json ParameterSchemaToJson(const Siligen::Domain::Recipes::ValueObjects::ParameterSchema& schema);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ParameterSchema> ParameterSchemaFromJson(
        const json& j);

    static json FieldChangeToJson(const Siligen::Domain::Recipes::ValueObjects::FieldChange& change);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::FieldChange> FieldChangeFromJson(
        const json& j);

    static json ParameterValueEntryToJson(const Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry& entry);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry>
    ParameterValueEntryFromJson(const json& j);

    static std::string ImportConflictTypeToString(Siligen::Domain::Recipes::ValueObjects::ImportConflictType type);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ImportConflictType>
    ImportConflictTypeFromString(const std::string& value);

    static std::string ConflictResolutionToString(Siligen::Domain::Recipes::ValueObjects::ConflictResolution resolution);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ConflictResolution>
    ConflictResolutionFromString(const std::string& value);

   private:
    static json ParameterValueToJson(const Siligen::Domain::Recipes::ValueObjects::ParameterValue& value);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ParameterValue> ParameterValueFromJson(
        const json& j);

    static std::string RecipeStatusToString(Siligen::Domain::Recipes::ValueObjects::RecipeStatus status);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::RecipeStatus>
    RecipeStatusFromString(const std::string& value);

    static std::string RecipeVersionStatusToString(Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus status);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus>
    RecipeVersionStatusFromString(const std::string& value);

    static std::string ParameterValueTypeToString(Siligen::Domain::Recipes::ValueObjects::ParameterValueType type);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::ParameterValueType>
    ParameterValueTypeFromString(const std::string& value);

    static std::string AuditActionToString(Siligen::Domain::Recipes::ValueObjects::AuditAction action);
    static Siligen::Shared::Types::Result<Siligen::Domain::Recipes::ValueObjects::AuditAction>
    AuditActionFromString(const std::string& value);

    static Siligen::Shared::Types::Result<std::string> GetRequiredString(const json& j, const char* field);
    static Siligen::Shared::Types::Result<std::int64_t> GetRequiredInt64(const json& j, const char* field);
};

}  // namespace Siligen::Domain::Recipes::Serialization
