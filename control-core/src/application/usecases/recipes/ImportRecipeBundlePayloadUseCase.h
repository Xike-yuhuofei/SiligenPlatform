#pragma once

#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeBundleSerializerPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/ports/ITemplateRepositoryPort.h"
#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Recipes {

struct ImportRecipeBundlePayloadRequest {
    std::string bundle_json;
    std::string resolution_strategy;
    bool dry_run = false;
    std::string actor;
    std::string source_label;
};

struct ImportRecipeBundlePayloadResponse {
    std::string status;
    std::vector<Siligen::Domain::Recipes::ValueObjects::ImportConflict> conflicts;
    size_t imported_count = 0;
};

class ImportRecipeBundlePayloadUseCase {
   public:
    ImportRecipeBundlePayloadUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port);

    Siligen::Shared::Types::Result<ImportRecipeBundlePayloadResponse> Execute(
        const ImportRecipeBundlePayloadRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port_;
};

}  // namespace Siligen::Application::UseCases::Recipes
