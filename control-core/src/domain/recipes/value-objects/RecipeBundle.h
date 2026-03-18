#pragma once

#include "domain/recipes/aggregates/Recipe.h"
#include "domain/recipes/aggregates/RecipeVersion.h"
#include "domain/recipes/value-objects/RecipeTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::ValueObjects {

struct RecipeBundle {
    std::string schema_version;
    std::int64_t exported_at;
    std::vector<Aggregates::Recipe> recipes;
    std::vector<Aggregates::RecipeVersion> versions;
    std::vector<Template> templates;
    std::vector<AuditRecord> audit_records;

    RecipeBundle() : exported_at(0) {}
};

}  // namespace Siligen::Domain::Recipes::ValueObjects
