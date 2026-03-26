#pragma once

#include "application/usecases/recipes/CompareRecipeVersionsUseCase.h"
#include "application/usecases/recipes/CreateDraftVersionUseCase.h"
#include "application/usecases/recipes/CreateRecipeUseCase.h"
#include "application/usecases/recipes/CreateVersionFromPublishedUseCase.h"
#include "application/usecases/recipes/ExportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/ImportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/RecipeCommandUseCase.h"
#include "application/usecases/recipes/RecipeQueryUseCase.h"
#include "application/usecases/recipes/UpdateDraftVersionUseCase.h"
#include "application/usecases/recipes/UpdateRecipeUseCase.h"

#include <memory>

namespace Siligen::Application::Facades::Tcp {

class TcpRecipeFacade {
   public:
    TcpRecipeFacade(std::shared_ptr<UseCases::Recipes::CreateRecipeUseCase> create_recipe_use_case,
                    std::shared_ptr<UseCases::Recipes::UpdateRecipeUseCase> update_recipe_use_case,
                    std::shared_ptr<UseCases::Recipes::CreateDraftVersionUseCase> create_draft_use_case,
                    std::shared_ptr<UseCases::Recipes::UpdateDraftVersionUseCase> update_draft_use_case,
                    std::shared_ptr<UseCases::Recipes::RecipeCommandUseCase> recipe_command_use_case,
                    std::shared_ptr<UseCases::Recipes::RecipeQueryUseCase> recipe_query_use_case,
                    std::shared_ptr<UseCases::Recipes::CreateVersionFromPublishedUseCase> create_version_use_case,
                    std::shared_ptr<UseCases::Recipes::CompareRecipeVersionsUseCase> compare_versions_use_case,
                    std::shared_ptr<UseCases::Recipes::ExportRecipeBundlePayloadUseCase> export_bundle_use_case,
                    std::shared_ptr<UseCases::Recipes::ImportRecipeBundlePayloadUseCase> import_bundle_use_case);

    Shared::Types::Result<UseCases::Recipes::ListRecipesResponse> ListRecipes(
        const UseCases::Recipes::ListRecipesRequest& request);
    Shared::Types::Result<UseCases::Recipes::GetRecipeResponse> GetRecipe(
        const UseCases::Recipes::GetRecipeRequest& request);
    Shared::Types::Result<UseCases::Recipes::CreateRecipeResponse> CreateRecipe(
        const UseCases::Recipes::CreateRecipeRequest& request);
    Shared::Types::Result<UseCases::Recipes::UpdateRecipeResponse> UpdateRecipe(
        const UseCases::Recipes::UpdateRecipeRequest& request);
    Shared::Types::Result<UseCases::Recipes::ArchiveRecipeResponse> ArchiveRecipe(
        const UseCases::Recipes::ArchiveRecipeRequest& request);
    Shared::Types::Result<UseCases::Recipes::CreateDraftVersionResponse> CreateDraft(
        const UseCases::Recipes::CreateDraftVersionRequest& request);
    Shared::Types::Result<UseCases::Recipes::UpdateDraftVersionResponse> UpdateDraft(
        const UseCases::Recipes::UpdateDraftVersionRequest& request);
    Shared::Types::Result<UseCases::Recipes::PublishRecipeVersionResponse> PublishRecipeVersion(
        const UseCases::Recipes::PublishRecipeVersionRequest& request);
    Shared::Types::Result<UseCases::Recipes::ListRecipeVersionsResponse> ListRecipeVersions(
        const UseCases::Recipes::ListRecipeVersionsRequest& request);
    Shared::Types::Result<UseCases::Recipes::CreateVersionFromPublishedResponse> CreateVersionFromPublished(
        const UseCases::Recipes::CreateVersionFromPublishedRequest& request);
    Shared::Types::Result<UseCases::Recipes::CompareRecipeVersionsResponse> CompareRecipeVersions(
        const UseCases::Recipes::CompareRecipeVersionsRequest& request);
    Shared::Types::Result<UseCases::Recipes::ActivateRecipeVersionResponse> ActivateRecipeVersion(
        const UseCases::Recipes::ActivateRecipeVersionRequest& request);
    Shared::Types::Result<UseCases::Recipes::ListTemplatesResponse> ListTemplates(
        const UseCases::Recipes::ListTemplatesRequest& request);
    Shared::Types::Result<UseCases::Recipes::GetDefaultParameterSchemaResponse> GetDefaultParameterSchema(
        const UseCases::Recipes::GetDefaultParameterSchemaRequest& request);
    Shared::Types::Result<UseCases::Recipes::GetRecipeAuditResponse> GetRecipeAudit(
        const UseCases::Recipes::GetRecipeAuditRequest& request);
    Shared::Types::Result<UseCases::Recipes::ExportRecipeBundlePayloadResponse> ExportBundle(
        const UseCases::Recipes::ExportRecipeBundlePayloadRequest& request);
    Shared::Types::Result<UseCases::Recipes::ImportRecipeBundlePayloadResponse> ImportBundle(
        const UseCases::Recipes::ImportRecipeBundlePayloadRequest& request);

   private:
    std::shared_ptr<UseCases::Recipes::CreateRecipeUseCase> create_recipe_use_case_;
    std::shared_ptr<UseCases::Recipes::UpdateRecipeUseCase> update_recipe_use_case_;
    std::shared_ptr<UseCases::Recipes::CreateDraftVersionUseCase> create_draft_use_case_;
    std::shared_ptr<UseCases::Recipes::UpdateDraftVersionUseCase> update_draft_use_case_;
    std::shared_ptr<UseCases::Recipes::RecipeCommandUseCase> recipe_command_use_case_;
    std::shared_ptr<UseCases::Recipes::RecipeQueryUseCase> recipe_query_use_case_;
    std::shared_ptr<UseCases::Recipes::CreateVersionFromPublishedUseCase> create_version_use_case_;
    std::shared_ptr<UseCases::Recipes::CompareRecipeVersionsUseCase> compare_versions_use_case_;
    std::shared_ptr<UseCases::Recipes::ExportRecipeBundlePayloadUseCase> export_bundle_use_case_;
    std::shared_ptr<UseCases::Recipes::ImportRecipeBundlePayloadUseCase> import_bundle_use_case_;
};

}  // namespace Siligen::Application::Facades::Tcp
