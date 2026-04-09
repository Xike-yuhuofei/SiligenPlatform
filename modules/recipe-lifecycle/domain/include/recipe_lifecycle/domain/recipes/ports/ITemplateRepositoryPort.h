#pragma once

#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Recipes::Ports {

using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::Template;
using Siligen::Domain::Recipes::ValueObjects::TemplateId;

class ITemplateRepositoryPort {
   public:
    virtual ~ITemplateRepositoryPort() = default;

    virtual Result<TemplateId> SaveTemplate(const Template& template_data) = 0;
    virtual Result<Template> GetTemplateById(const TemplateId& template_id) const = 0;
    virtual Result<Template> GetTemplateByName(const std::string& name) const = 0;
    virtual Result<std::vector<Template>> ListTemplates() const = 0;
};

}  // namespace Siligen::Domain::Recipes::Ports
