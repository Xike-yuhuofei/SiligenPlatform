#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/ParameterSchema.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::ValueObjects {

using RecipeId = std::string;
using RecipeVersionId = std::string;
using TemplateId = std::string;
using AuditRecordId = std::string;

enum class RecipeStatus {
    Active,
    Archived
};

enum class RecipeVersionStatus {
    Draft,
    Published,
    Archived
};

struct ParameterValueEntry {
    std::string key;
    ParameterValue value;
};

struct RecipeMetadata {
    RecipeId id;
    std::string name;
    std::string description;
    RecipeStatus status;
    std::vector<std::string> tags;
    RecipeVersionId active_version_id;
    std::int64_t created_at;
    std::int64_t updated_at;

    RecipeMetadata() : status(RecipeStatus::Active), created_at(0), updated_at(0) {}
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
    RecipeVersionStatus status;
    RecipeVersionId base_version_id;
    std::vector<ParameterValueEntry> parameters;
    std::string created_by;
    std::int64_t created_at;
    std::optional<std::int64_t> published_at;
    std::string change_note;

    RecipeVersionMetadata() : status(RecipeVersionStatus::Draft), created_at(0) {}
};

struct Template {
    TemplateId id;
    std::string name;
    std::string description;
    std::vector<ParameterValueEntry> parameters;
    std::int64_t created_at;
    std::int64_t updated_at;

    Template() : created_at(0), updated_at(0) {}
};

enum class AuditAction {
    Create,
    Edit,
    Publish,
    Rollback,
    Archive,
    Import,
    Export
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
    AuditAction action;
    std::string actor;
    std::int64_t timestamp;
    std::vector<FieldChange> changes;

    AuditRecord() : action(AuditAction::Create), timestamp(0) {}
};

enum class ImportConflictType {
    Name,
    Version
};

enum class ConflictResolution {
    Rename,
    Skip,
    Replace
};

struct ImportConflict {
    ImportConflictType type;
    std::string message;
    RecipeId existing_id;
    std::string incoming_name;
    ConflictResolution suggested_resolution;
};

}  // namespace Siligen::Domain::Recipes::ValueObjects
