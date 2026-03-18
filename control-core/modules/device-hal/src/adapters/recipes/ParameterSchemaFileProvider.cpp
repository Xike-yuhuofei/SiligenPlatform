#include "ParameterSchemaFileProvider.h"

#include "RecipeJsonSerializer.h"
#include "shared/types/Error.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ParameterSchemaFileProvider"

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Infrastructure::Adapters::Recipes::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::ParameterSchema;

Result<void> EnsureDirectory(const std::string& directory) {
    if (directory.empty()) {
        return Result<void>::Success();
    }

    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR,
                                           "Failed to create directory: " + directory + ", " + ec.message(),
                                           MODULE_NAME));
    }
    return Result<void>::Success();
}

Result<std::string> ReadFile(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<std::string>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + filepath.string(), MODULE_NAME));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return Result<std::string>::Success(buffer.str());
}

Result<ParameterSchema> LoadSchemaFile(const std::filesystem::path& path) {
    auto content = ReadFile(path);
    if (content.IsError()) {
        return Result<ParameterSchema>::Failure(content.GetError());
    }

    auto json_result = RecipeJsonSerializer::Parse(content.Value());
    if (json_result.IsError()) {
        return Result<ParameterSchema>::Failure(json_result.GetError());
    }

    return RecipeJsonSerializer::ParameterSchemaFromJson(json_result.Value());
}

Result<std::vector<ParameterSchema>> LoadSchemasFromDirectory(const std::string& directory) {
    std::vector<ParameterSchema> schemas;

    if (directory.empty()) {
        return Result<std::vector<ParameterSchema>>::Success(schemas);
    }

    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) || ec) {
        return Result<std::vector<ParameterSchema>>::Success(schemas);
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            return Result<std::vector<ParameterSchema>>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "Failed to iterate directory: " + directory, MODULE_NAME));
        }
        if (entry.is_regular_file()) {
            const auto& path = entry.path();
            if (path.extension() == ".json") {
                files.push_back(path);
            }
        }
    }

    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        auto schema_result = LoadSchemaFile(path);
        if (schema_result.IsError()) {
            return Result<std::vector<ParameterSchema>>::Failure(schema_result.GetError());
        }
        schemas.push_back(schema_result.Value());
    }

    return Result<std::vector<ParameterSchema>>::Success(schemas);
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Recipes {

ParameterSchemaFileProvider::ParameterSchemaFileProvider(const std::string& primary_directory,
                                                         const std::string& fallback_directory)
    : primary_directory_(primary_directory),
      fallback_directory_(fallback_directory) {
    auto ensure_result = EnsureDirectory(primary_directory_);
    if (ensure_result.IsError()) {
        SILIGEN_LOG_WARNING("Failed to ensure schema directory: " + ensure_result.GetError().GetMessage());
    }
}

Result<ParameterSchema> ParameterSchemaFileProvider::GetSchemaById(const std::string& schema_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto primary_result = LoadSchemasFromDirectory(primary_directory_);
    if (primary_result.IsError()) {
        return Result<ParameterSchema>::Failure(primary_result.GetError());
    }

    for (const auto& schema : primary_result.Value()) {
        if (schema.schema_id == schema_id) {
            return Result<ParameterSchema>::Success(schema);
        }
    }

    auto fallback_result = LoadSchemasFromDirectory(fallback_directory_);
    if (fallback_result.IsError()) {
        return Result<ParameterSchema>::Failure(fallback_result.GetError());
    }

    for (const auto& schema : fallback_result.Value()) {
        if (schema.schema_id == schema_id) {
            return Result<ParameterSchema>::Success(schema);
        }
    }

    return Result<ParameterSchema>::Failure(
        Error(ErrorCode::PARAMETER_SCHEMA_NOT_FOUND, "Schema not found: " + schema_id, MODULE_NAME));
}

Result<ParameterSchema> ParameterSchemaFileProvider::GetDefaultSchema() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto primary_result = LoadSchemasFromDirectory(primary_directory_);
    if (primary_result.IsError()) {
        return Result<ParameterSchema>::Failure(primary_result.GetError());
    }
    auto fallback_result = LoadSchemasFromDirectory(fallback_directory_);
    if (fallback_result.IsError()) {
        return Result<ParameterSchema>::Failure(fallback_result.GetError());
    }

    for (const auto& schema : primary_result.Value()) {
        if (schema.schema_id == "default") {
            return Result<ParameterSchema>::Success(schema);
        }
    }
    for (const auto& schema : fallback_result.Value()) {
        if (schema.schema_id == "default") {
            return Result<ParameterSchema>::Success(schema);
        }
    }

    if (!primary_result.Value().empty()) {
        return Result<ParameterSchema>::Success(primary_result.Value().front());
    }
    if (!fallback_result.Value().empty()) {
        return Result<ParameterSchema>::Success(fallback_result.Value().front());
    }

    return Result<ParameterSchema>::Failure(
        Error(ErrorCode::PARAMETER_SCHEMA_NOT_FOUND, "No parameter schemas available", MODULE_NAME));
}

Result<std::vector<ParameterSchema>> ParameterSchemaFileProvider::ListSchemas() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto primary_result = LoadSchemasFromDirectory(primary_directory_);
    if (primary_result.IsError()) {
        return Result<std::vector<ParameterSchema>>::Failure(primary_result.GetError());
    }
    auto fallback_result = LoadSchemasFromDirectory(fallback_directory_);
    if (fallback_result.IsError()) {
        return Result<std::vector<ParameterSchema>>::Failure(fallback_result.GetError());
    }

    std::vector<ParameterSchema> combined;
    std::unordered_set<std::string> seen;
    for (const auto& schema : primary_result.Value()) {
        combined.push_back(schema);
        seen.insert(schema.schema_id);
    }
    for (const auto& schema : fallback_result.Value()) {
        if (seen.find(schema.schema_id) == seen.end()) {
            combined.push_back(schema);
            seen.insert(schema.schema_id);
        }
    }

    if (combined.empty()) {
        return Result<std::vector<ParameterSchema>>::Failure(
            Error(ErrorCode::PARAMETER_SCHEMA_NOT_FOUND, "No parameter schemas available", MODULE_NAME));
    }

    return Result<std::vector<ParameterSchema>>::Success(combined);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
