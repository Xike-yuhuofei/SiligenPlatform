#include "TcpRecipeFacade.h"

namespace Siligen::Application::Facades::Tcp {

TcpRecipeFacade::TcpRecipeFacade(
    std::shared_ptr<UseCases::Recipes::CreateRecipeUseCase> create_recipe_use_case,
    std::shared_ptr<UseCases::Recipes::UpdateRecipeUseCase> update_recipe_use_case,
    std::shared_ptr<UseCases::Recipes::CreateDraftVersionUseCase> create_draft_use_case,
    std::shared_ptr<UseCases::Recipes::UpdateDraftVersionUseCase> update_draft_use_case,
    std::shared_ptr<UseCases::Recipes::RecipeCommandUseCase> recipe_command_use_case,
    std::shared_ptr<UseCases::Recipes::RecipeQueryUseCase> recipe_query_use_case,
    std::shared_ptr<UseCases::Recipes::CreateVersionFromPublishedUseCase> create_version_use_case,
    std::shared_ptr<UseCases::Recipes::CompareRecipeVersionsUseCase> compare_versions_use_case,
    std::shared_ptr<UseCases::Recipes::ExportRecipeBundlePayloadUseCase> export_bundle_use_case,
    std::shared_ptr<UseCases::Recipes::ImportRecipeBundlePayloadUseCase> import_bundle_use_case)
    : create_recipe_use_case_(std::move(create_recipe_use_case)),
      update_recipe_use_case_(std::move(update_recipe_use_case)),
      create_draft_use_case_(std::move(create_draft_use_case)),
      update_draft_use_case_(std::move(update_draft_use_case)),
      recipe_command_use_case_(std::move(recipe_command_use_case)),
      recipe_query_use_case_(std::move(recipe_query_use_case)),
      create_version_use_case_(std::move(create_version_use_case)),
      compare_versions_use_case_(std::move(compare_versions_use_case)),
      export_bundle_use_case_(std::move(export_bundle_use_case)),
      import_bundle_use_case_(std::move(import_bundle_use_case)) {}

Shared::Types::Result<UseCases::Recipes::ListRecipesResponse> TcpRecipeFacade::ListRecipes(
    const UseCases::Recipes::ListRecipesRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::GetRecipeResponse> TcpRecipeFacade::GetRecipe(
    const UseCases::Recipes::GetRecipeRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::CreateRecipeResponse> TcpRecipeFacade::CreateRecipe(
    const UseCases::Recipes::CreateRecipeRequest& request) {
    return create_recipe_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::UpdateRecipeResponse> TcpRecipeFacade::UpdateRecipe(
    const UseCases::Recipes::UpdateRecipeRequest& request) {
    return update_recipe_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ArchiveRecipeResponse> TcpRecipeFacade::ArchiveRecipe(
    const UseCases::Recipes::ArchiveRecipeRequest& request) {
    return recipe_command_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::CreateDraftVersionResponse> TcpRecipeFacade::CreateDraft(
    const UseCases::Recipes::CreateDraftVersionRequest& request) {
    return create_draft_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::UpdateDraftVersionResponse> TcpRecipeFacade::UpdateDraft(
    const UseCases::Recipes::UpdateDraftVersionRequest& request) {
    return update_draft_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::PublishRecipeVersionResponse> TcpRecipeFacade::PublishRecipeVersion(
    const UseCases::Recipes::PublishRecipeVersionRequest& request) {
    return recipe_command_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ListRecipeVersionsResponse> TcpRecipeFacade::ListRecipeVersions(
    const UseCases::Recipes::ListRecipeVersionsRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::CreateVersionFromPublishedResponse>
TcpRecipeFacade::CreateVersionFromPublished(const UseCases::Recipes::CreateVersionFromPublishedRequest& request) {
    return create_version_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::CompareRecipeVersionsResponse> TcpRecipeFacade::CompareRecipeVersions(
    const UseCases::Recipes::CompareRecipeVersionsRequest& request) {
    return compare_versions_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ActivateRecipeVersionResponse> TcpRecipeFacade::ActivateRecipeVersion(
    const UseCases::Recipes::ActivateRecipeVersionRequest& request) {
    return recipe_command_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ListTemplatesResponse> TcpRecipeFacade::ListTemplates(
    const UseCases::Recipes::ListTemplatesRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::GetDefaultParameterSchemaResponse> TcpRecipeFacade::GetDefaultParameterSchema(
    const UseCases::Recipes::GetDefaultParameterSchemaRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::GetRecipeAuditResponse> TcpRecipeFacade::GetRecipeAudit(
    const UseCases::Recipes::GetRecipeAuditRequest& request) {
    return recipe_query_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ExportRecipeBundlePayloadResponse> TcpRecipeFacade::ExportBundle(
    const UseCases::Recipes::ExportRecipeBundlePayloadRequest& request) {
    return export_bundle_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Recipes::ImportRecipeBundlePayloadResponse> TcpRecipeFacade::ImportBundle(
    const UseCases::Recipes::ImportRecipeBundlePayloadRequest& request) {
    return import_bundle_use_case_->Execute(request);
}

}  // namespace Siligen::Application::Facades::Tcp
