#pragma once

#include "domain/recipes/ports/IAuditRepositoryPort.h"
#include "domain/recipes/ports/IRecipeBundleSerializerPort.h"
#include "domain/recipes/ports/IRecipeRepositoryPort.h"
#include "domain/recipes/ports/ITemplateRepositoryPort.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>

namespace Siligen::Application::UseCases::Recipes {

struct ExportRecipeBundlePayloadRequest {
    std::string recipe_id;
};

struct ExportRecipeBundlePayloadResponse {
    std::string bundle_json;
    size_t recipe_count = 0;
    size_t version_count = 0;
    size_t audit_count = 0;
};

class ExportRecipeBundlePayloadUseCase {
   public:
    ExportRecipeBundlePayloadUseCase(
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository,
        std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port);

    Siligen::Shared::Types::Result<ExportRecipeBundlePayloadResponse> Execute(
        const ExportRecipeBundlePayloadRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::ITemplateRepositoryPort> template_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IAuditRepositoryPort> audit_repository_;
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeBundleSerializerPort> serializer_port_;
};

}  // namespace Siligen::Application::UseCases::Recipes
