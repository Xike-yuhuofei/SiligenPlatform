#include "RecipeFileRepository.h"

#include "recipes/serialization/RecipeJsonSerializer.h"
#include "shared/types/Error.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "RecipeFileRepository"

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Infrastructure::Adapters::Recipes::RecipeJsonSerializer;
using Siligen::Domain::Recipes::Aggregates::Recipe;
using Siligen::Domain::Recipes::Aggregates::RecipeVersion;

std::int64_t NowTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ContainsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    auto hay = ToLowerCopy(haystack);
    auto ned = ToLowerCopy(needle);
    return hay.find(ned) != std::string::npos;
}

Result<Recipe> ParseRecipeRoot(const RecipeJsonSerializer::json& root) {
    if (root.contains("recipe") && root.at("recipe").is_object()) {
        return RecipeJsonSerializer::RecipeFromJson(root.at("recipe"));
    }
    return RecipeJsonSerializer::RecipeFromJson(root);
}

Result<RecipeVersion> ParseVersionRoot(const RecipeJsonSerializer::json& root) {
    if (root.contains("version") && root.at("version").is_object()) {
        return RecipeJsonSerializer::RecipeVersionFromJson(root.at("version"));
    }
    return RecipeJsonSerializer::RecipeVersionFromJson(root);
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Recipes {

RecipeFileRepository::RecipeFileRepository(const std::string& base_directory)
    : base_directory_(base_directory),
      recipes_directory_(std::filesystem::path(base_directory_) / "recipes"),
      versions_directory_(std::filesystem::path(base_directory_) / "versions") {}

Result<void> RecipeFileRepository::EnsureDirectories() const {
    std::error_code ec;
    std::filesystem::create_directories(recipes_directory_, ec);
    if (ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "Failed to create recipes directory: " + recipes_directory_.string() + ", " + ec.message(),
                  MODULE_NAME));
    }

    std::filesystem::create_directories(versions_directory_, ec);
    if (ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "Failed to create versions directory: " + versions_directory_.string() + ", " + ec.message(),
                  MODULE_NAME));
    }

    return Result<void>::Success();
}

Result<void> RecipeFileRepository::WriteJsonFile(const std::filesystem::path& path,
                                                 const nlohmann::json& data) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR,
                  "Failed to create directory: " + path.parent_path().string() + ", " + ec.message(),
                  MODULE_NAME));
    }

    auto dump_result = RecipeJsonSerializer::Dump(data);
    if (dump_result.IsError()) {
        return Result<void>::Failure(dump_result.GetError());
    }

    std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Unable to write file: " + path.string(), MODULE_NAME));
    }

    file << dump_result.Value();
    if (!file.good()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Write failed for file: " + path.string(), MODULE_NAME));
    }

    return Result<void>::Success();
}

Result<nlohmann::json> RecipeFileRepository::ReadJsonFile(const std::filesystem::path& path) const {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<nlohmann::json>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + path.string(), MODULE_NAME));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto parse_result = RecipeJsonSerializer::Parse(buffer.str());
    if (parse_result.IsError()) {
        return Result<nlohmann::json>::Failure(parse_result.GetError());
    }

    return Result<nlohmann::json>::Success(parse_result.Value());
}

std::filesystem::path RecipeFileRepository::RecipePath(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const {
    return recipes_directory_ / (recipe_id + ".json");
}

std::filesystem::path RecipeFileRepository::VersionPath(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
    const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id) const {
    return versions_directory_ / recipe_id / (version_id + ".json");
}

bool RecipeFileRepository::RecipeMatchesFilter(
    const Recipe& recipe,
    const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& filter) const {
    if (filter.status.has_value() && recipe.status != filter.status.value()) {
        return false;
    }

    if (!filter.query.empty()) {
        if (!ContainsInsensitive(recipe.name, filter.query) &&
            !ContainsInsensitive(recipe.description, filter.query)) {
            return false;
        }
    }

    if (!filter.tag.empty()) {
        const auto needle = ToLowerCopy(filter.tag);
        bool found = false;
        for (const auto& tag : recipe.tags) {
            if (ToLowerCopy(tag) == needle) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

Result<Siligen::Domain::Recipes::ValueObjects::RecipeId> RecipeFileRepository::SaveRecipe(
    const Recipe& recipe) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recipe.id.empty()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", MODULE_NAME));
    }

    auto ensure_result = EnsureDirectories();
    if (ensure_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(ensure_result.GetError());
    }

    if (!recipe.name.empty() && std::filesystem::exists(recipes_directory_)) {
        for (const auto& entry : std::filesystem::directory_iterator(recipes_directory_)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto json_result = ReadJsonFile(entry.path());
            if (json_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(json_result.GetError());
            }
            auto recipe_result = ParseRecipeRoot(json_result.Value());
            if (recipe_result.IsError()) {
                return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(recipe_result.GetError());
            }
            const auto& existing = recipe_result.Value();
            if (ToLowerCopy(existing.name) == ToLowerCopy(recipe.name) && existing.id != recipe.id) {
                return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(
                    Error(ErrorCode::RECIPE_ALREADY_EXISTS, "Recipe name already exists: " + recipe.name, MODULE_NAME));
            }
        }
    }

    Recipe updated = recipe;
    if (updated.created_at == 0) {
        updated.created_at = NowTimestamp();
    }
    if (updated.updated_at == 0) {
        updated.updated_at = updated.created_at;
    }

    RecipeJsonSerializer::json root;
    root["schemaVersion"] = "1.0";
    root["recipe"] = RecipeJsonSerializer::RecipeToJson(updated);

    auto write_result = WriteJsonFile(RecipePath(updated.id), root);
    if (write_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Failure(write_result.GetError());
    }

    return Result<Siligen::Domain::Recipes::ValueObjects::RecipeId>::Success(updated.id);
}

Result<Recipe> RecipeFileRepository::GetRecipeById(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recipe_id.empty()) {
        return Result<Recipe>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", MODULE_NAME));
    }

    auto path = RecipePath(recipe_id);
    if (!std::filesystem::exists(path)) {
        return Result<Recipe>::Failure(
            Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + recipe_id, MODULE_NAME));
    }

    auto json_result = ReadJsonFile(path);
    if (json_result.IsError()) {
        return Result<Recipe>::Failure(json_result.GetError());
    }
    auto recipe_result = ParseRecipeRoot(json_result.Value());
    if (recipe_result.IsError()) {
        return Result<Recipe>::Failure(recipe_result.GetError());
    }

    return Result<Recipe>::Success(recipe_result.Value());
}

Result<Recipe> RecipeFileRepository::GetRecipeByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (name.empty()) {
        return Result<Recipe>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe name is required", MODULE_NAME));
    }

    if (!std::filesystem::exists(recipes_directory_)) {
        return Result<Recipe>::Failure(
            Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + name, MODULE_NAME));
    }

    auto needle = ToLowerCopy(name);
    for (const auto& entry : std::filesystem::directory_iterator(recipes_directory_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto json_result = ReadJsonFile(entry.path());
        if (json_result.IsError()) {
            return Result<Recipe>::Failure(json_result.GetError());
        }
        auto recipe_result = ParseRecipeRoot(json_result.Value());
        if (recipe_result.IsError()) {
            return Result<Recipe>::Failure(recipe_result.GetError());
        }
        const auto& recipe = recipe_result.Value();
        if (ToLowerCopy(recipe.name) == needle) {
            return Result<Recipe>::Success(recipe);
        }
    }

    return Result<Recipe>::Failure(
        Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + name, MODULE_NAME));
}

Result<std::vector<Recipe>> RecipeFileRepository::ListRecipes(
    const Siligen::Domain::Recipes::ValueObjects::RecipeListFilter& filter) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Recipe> results;
    if (!std::filesystem::exists(recipes_directory_)) {
        return Result<std::vector<Recipe>>::Success(results);
    }

    for (const auto& entry : std::filesystem::directory_iterator(recipes_directory_)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto json_result = ReadJsonFile(entry.path());
        if (json_result.IsError()) {
            return Result<std::vector<Recipe>>::Failure(json_result.GetError());
        }
        auto recipe_result = ParseRecipeRoot(json_result.Value());
        if (recipe_result.IsError()) {
            return Result<std::vector<Recipe>>::Failure(recipe_result.GetError());
        }
        const auto& recipe = recipe_result.Value();
        if (RecipeMatchesFilter(recipe, filter)) {
            results.push_back(recipe);
        }
    }

    return Result<std::vector<Recipe>>::Success(results);
}

Result<void> RecipeFileRepository::ArchiveRecipe(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recipe_id.empty()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", MODULE_NAME));
    }

    auto path = RecipePath(recipe_id);
    if (!std::filesystem::exists(path)) {
        return Result<void>::Failure(
            Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + recipe_id, MODULE_NAME));
    }

    auto json_result = ReadJsonFile(path);
    if (json_result.IsError()) {
        return Result<void>::Failure(json_result.GetError());
    }
    auto recipe_result = ParseRecipeRoot(json_result.Value());
    if (recipe_result.IsError()) {
        return Result<void>::Failure(recipe_result.GetError());
    }

    auto recipe = recipe_result.Value();
    recipe.Archive();
    recipe.updated_at = NowTimestamp();

    RecipeJsonSerializer::json root;
    root["schemaVersion"] = "1.0";
    root["recipe"] = RecipeJsonSerializer::RecipeToJson(recipe);
    auto write_result = WriteJsonFile(path, root);
    if (write_result.IsError()) {
        return Result<void>::Failure(write_result.GetError());
    }

    return Result<void>::Success();
}

Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId> RecipeFileRepository::SaveVersion(
    const RecipeVersion& version) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (version.id.empty() || version.recipe_id.empty()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version id are required", MODULE_NAME));
    }

    auto ensure_result = EnsureDirectories();
    if (ensure_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(ensure_result.GetError());
    }

    auto recipe_path = RecipePath(version.recipe_id);
    if (!std::filesystem::exists(recipe_path)) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(
            Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + version.recipe_id, MODULE_NAME));
    }

    auto recipe_json = ReadJsonFile(recipe_path);
    if (recipe_json.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(recipe_json.GetError());
    }
    auto recipe_result = ParseRecipeRoot(recipe_json.Value());
    if (recipe_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(recipe_result.GetError());
    }

    RecipeVersion updated = version;
    if (updated.created_at == 0) {
        updated.created_at = NowTimestamp();
    }

    RecipeJsonSerializer::json root;
    root["schemaVersion"] = "1.0";
    root["version"] = RecipeJsonSerializer::RecipeVersionToJson(updated);
    auto write_result = WriteJsonFile(VersionPath(updated.recipe_id, updated.id), root);
    if (write_result.IsError()) {
        return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(write_result.GetError());
    }

    auto recipe = recipe_result.Value();
    bool has_version = false;
    for (const auto& existing_id : recipe.version_ids) {
        if (existing_id == updated.id) {
            has_version = true;
            break;
        }
    }
    if (!has_version) {
        recipe.version_ids.push_back(updated.id);

        RecipeJsonSerializer::json recipe_root;
        recipe_root["schemaVersion"] = "1.0";
        recipe_root["recipe"] = RecipeJsonSerializer::RecipeToJson(recipe);
        auto recipe_write = WriteJsonFile(recipe_path, recipe_root);
        if (recipe_write.IsError()) {
            return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Failure(recipe_write.GetError());
        }
    }

    return Result<Siligen::Domain::Recipes::ValueObjects::RecipeVersionId>::Success(updated.id);
}

Result<RecipeVersion> RecipeFileRepository::GetVersionById(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id,
    const Siligen::Domain::Recipes::ValueObjects::RecipeVersionId& version_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recipe_id.empty() || version_id.empty()) {
        return Result<RecipeVersion>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id and version id are required", MODULE_NAME));
    }

    auto path = VersionPath(recipe_id, version_id);
    if (!std::filesystem::exists(path)) {
        return Result<RecipeVersion>::Failure(
            Error(ErrorCode::RECIPE_VERSION_NOT_FOUND, "Recipe version not found: " + version_id, MODULE_NAME));
    }

    auto json_result = ReadJsonFile(path);
    if (json_result.IsError()) {
        return Result<RecipeVersion>::Failure(json_result.GetError());
    }
    auto version_result = ParseVersionRoot(json_result.Value());
    if (version_result.IsError()) {
        return Result<RecipeVersion>::Failure(version_result.GetError());
    }

    return Result<RecipeVersion>::Success(version_result.Value());
}

Result<std::vector<RecipeVersion>> RecipeFileRepository::ListVersions(
    const Siligen::Domain::Recipes::ValueObjects::RecipeId& recipe_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (recipe_id.empty()) {
        return Result<std::vector<RecipeVersion>>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Recipe id is required", MODULE_NAME));
    }

    auto recipe_path = RecipePath(recipe_id);
    if (!std::filesystem::exists(recipe_path)) {
        return Result<std::vector<RecipeVersion>>::Failure(
            Error(ErrorCode::RECIPE_NOT_FOUND, "Recipe not found: " + recipe_id, MODULE_NAME));
    }

    std::vector<RecipeVersion> results;
    auto version_dir = versions_directory_ / recipe_id;
    if (!std::filesystem::exists(version_dir)) {
        return Result<std::vector<RecipeVersion>>::Success(results);
    }

    for (const auto& entry : std::filesystem::directory_iterator(version_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto json_result = ReadJsonFile(entry.path());
        if (json_result.IsError()) {
            return Result<std::vector<RecipeVersion>>::Failure(json_result.GetError());
        }
        auto version_result = ParseVersionRoot(json_result.Value());
        if (version_result.IsError()) {
            return Result<std::vector<RecipeVersion>>::Failure(version_result.GetError());
        }
        results.push_back(version_result.Value());
    }

    return Result<std::vector<RecipeVersion>>::Success(results);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
