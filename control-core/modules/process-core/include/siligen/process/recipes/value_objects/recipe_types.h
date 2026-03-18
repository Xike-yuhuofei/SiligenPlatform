#pragma once

#include "siligen/process/recipes/value_objects/parameter_schema.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Process::Recipes::ValueObjects {

using RecipeId = std::string;
using RecipeVersionId = std::string;
using TemplateId = std::string;
using AuditRecordId = std::string;

enum class RecipeStatus {
    Active,
    Archived,
};

enum class RecipeVersionStatus {
    Draft,
    Published,
    Archived,
};

struct ParameterValueEntry {
    std::string key;
    ParameterValue value;
};

struct RecipeMetadata {
    RecipeId id;
    std::string name;
    std::string description;
    RecipeStatus status = RecipeStatus::Active;
    std::vector<std::string> tags;
    RecipeVersionId active_version_id;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
};

struct RecipeListFilter {
    std::optional<RecipeStatus> status;
    std::string query;
    std::string tag;
};

struct RecipeVersionMetadata {
    RecipeVersionId id;
    RecipeId recipe_id;
    std::string version_label;
    RecipeVersionStatus status = RecipeVersionStatus::Draft;
    RecipeVersionId base_version_id;
    std::vector<ParameterValueEntry> parameters;
    std::string created_by;
    std::int64_t created_at = 0;
    std::optional<std::int64_t> published_at;
    std::string change_note;
};

struct Template {
    TemplateId id;
    std::string name;
    std::string description;
    std::vector<ParameterValueEntry> parameters;
    std::int64_t created_at = 0;
    std::int64_t updated_at = 0;
};

enum class AuditAction {
    Create,
    Edit,
    Publish,
    Rollback,
    Archive,
    Import,
    Export,
};

struct FieldChange {
    std::string field;
    std::string before;
    std::string after;
};

struct AuditRecord {
    AuditRecordId id;
    RecipeId recipe_id;
    std::optional<RecipeVersionId> version_id;
    AuditAction action = AuditAction::Create;
    std::string actor;
    std::int64_t timestamp = 0;
    std::vector<FieldChange> changes;
};

}  // namespace Siligen::Process::Recipes::ValueObjects
