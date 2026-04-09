#include "ApplicationContainer.h"

#include "recipe_lifecycle/application/usecases/recipes/CompareRecipeVersionsUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/CreateDraftVersionUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/CreateRecipeUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/CreateVersionFromPublishedUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/ExportRecipeBundlePayloadUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/ImportRecipeBundlePayloadUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/RecipeCommandUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/RecipeQueryUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/UpdateDraftVersionUseCase.h"
#include "recipe_lifecycle/application/usecases/recipes/UpdateRecipeUseCase.h"
#include "shared/interfaces/ILoggingService.h"

#include <memory>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Recipes"

namespace Siligen::Application::Container {

void ApplicationContainer::ValidateRecipePorts() {
    if (!recipe_repository_) {
        SILIGEN_LOG_WARNING("IRecipeRepositoryPort 未注册");
    }
    if (!template_repository_) {
        SILIGEN_LOG_WARNING("ITemplateRepositoryPort 未注册");
    }
    if (!audit_repository_) {
        SILIGEN_LOG_WARNING("IAuditRepositoryPort 未注册");
    }
    if (!parameter_schema_port_) {
        SILIGEN_LOG_WARNING("IParameterSchemaPort 未注册");
    }
    if (!recipe_bundle_serializer_port_) {
        SILIGEN_LOG_WARNING("IRecipeBundleSerializerPort 未注册");
    }
}

void ApplicationContainer::ConfigureRecipeServices() {
    // recipe 校验/激活当前由 recipe-lifecycle owner 模块提供，这里不再注册 workflow 兼容域服务。
}

template<>
std::shared_ptr<UseCases::Recipes::CreateRecipeUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateRecipeUseCase>() {
    return std::make_shared<UseCases::Recipes::CreateRecipeUseCase>(
        recipe_repository_,
        audit_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::UpdateRecipeUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::UpdateRecipeUseCase>() {
    return std::make_shared<UseCases::Recipes::UpdateRecipeUseCase>(
        recipe_repository_,
        audit_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::CreateDraftVersionUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateDraftVersionUseCase>() {
    return std::make_shared<UseCases::Recipes::CreateDraftVersionUseCase>(
        recipe_repository_,
        template_repository_,
        parameter_schema_port_,
        audit_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::UpdateDraftVersionUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::UpdateDraftVersionUseCase>() {
    return std::make_shared<UseCases::Recipes::UpdateDraftVersionUseCase>(
        recipe_repository_,
        parameter_schema_port_,
        audit_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::RecipeCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::RecipeCommandUseCase>() {
    return std::make_shared<UseCases::Recipes::RecipeCommandUseCase>(
        recipe_repository_,
        audit_repository_,
        parameter_schema_port_);
}

template<>
std::shared_ptr<UseCases::Recipes::RecipeQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::RecipeQueryUseCase>() {
    return std::make_shared<UseCases::Recipes::RecipeQueryUseCase>(
        recipe_repository_,
        template_repository_,
        audit_repository_,
        parameter_schema_port_);
}

template<>
std::shared_ptr<UseCases::Recipes::CreateVersionFromPublishedUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CreateVersionFromPublishedUseCase>() {
    return std::make_shared<UseCases::Recipes::CreateVersionFromPublishedUseCase>(
        recipe_repository_,
        parameter_schema_port_,
        audit_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::CompareRecipeVersionsUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::CompareRecipeVersionsUseCase>() {
    return std::make_shared<UseCases::Recipes::CompareRecipeVersionsUseCase>(recipe_repository_);
}

template<>
std::shared_ptr<UseCases::Recipes::ExportRecipeBundlePayloadUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::ExportRecipeBundlePayloadUseCase>() {
    return std::make_shared<UseCases::Recipes::ExportRecipeBundlePayloadUseCase>(
        recipe_repository_,
        template_repository_,
        audit_repository_,
        recipe_bundle_serializer_port_);
}

template<>
std::shared_ptr<UseCases::Recipes::ImportRecipeBundlePayloadUseCase>
ApplicationContainer::CreateInstance<UseCases::Recipes::ImportRecipeBundlePayloadUseCase>() {
    return std::make_shared<UseCases::Recipes::ImportRecipeBundlePayloadUseCase>(
        recipe_repository_,
        template_repository_,
        audit_repository_,
        recipe_bundle_serializer_port_);
}

}  // namespace Siligen::Application::Container
