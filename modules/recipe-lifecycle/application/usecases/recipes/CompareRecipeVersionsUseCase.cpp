#include "recipe_lifecycle/application/usecases/recipes/CompareRecipeVersionsUseCase.h"

#include "RecipeUseCaseHelpers.h"
#include "shared/types/Error.h"

#include <unordered_map>

namespace Siligen::Application::UseCases::Recipes {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Recipes::ValueObjects::FieldChange;

CompareRecipeVersionsUseCase::CompareRecipeVersionsUseCase(
    std::shared_ptr<Siligen::Domain::Recipes::Ports::IRecipeRepositoryPort> recipe_repository)
    : recipe_repository_(std::move(recipe_repository)) {}

Result<CompareRecipeVersionsResponse> CompareRecipeVersionsUseCase::Execute(
    const CompareRecipeVersionsRequest& request) {
    if (!recipe_repository_) {
        return Result<CompareRecipeVersionsResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Recipe repository not available", "CompareRecipeVersionsUseCase"));
    }
    if (request.recipe_id.empty() || request.from_version_id.empty() || request.to_version_id.empty()) {
        return Result<CompareRecipeVersionsResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version ids are required", "CompareRecipeVersionsUseCase"));
    }

    auto from_result = recipe_repository_->GetVersionById(request.recipe_id, request.from_version_id);
    if (from_result.IsError()) {
        return Result<CompareRecipeVersionsResponse>::Failure(from_result.GetError());
    }
    auto to_result = recipe_repository_->GetVersionById(request.recipe_id, request.to_version_id);
    if (to_result.IsError()) {
        return Result<CompareRecipeVersionsResponse>::Failure(to_result.GetError());
    }

    auto changes = DiffParameters(from_result.Value().parameters, to_result.Value().parameters);
    CompareRecipeVersionsResponse response;
    response.recipe_id = request.recipe_id;
    response.from_version_id = request.from_version_id;
    response.to_version_id = request.to_version_id;
    response.changes = std::move(changes);
    return Result<CompareRecipeVersionsResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::Recipes

