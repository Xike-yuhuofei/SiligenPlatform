#include "TemplateFileRepository.h"

#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
#include "shared/types/Error.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "TemplateFileRepository"

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Infrastructure::Adapters::Recipes::RecipeJsonSerializer;
using Siligen::Domain::Recipes::ValueObjects::Template;
using Siligen::Domain::Recipes::ValueObjects::TemplateId;

std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string GenerateId(const std::string& prefix) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dist;

    std::ostringstream oss;
    oss << prefix << "_" << now << "_" << std::hex << dist(gen);
    return oss.str();
}

std::int64_t NowTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::filesystem::path LegacyTemplateFilePath(const std::filesystem::path& base_directory) {
    return base_directory / (std::string("templates") + ".json");
}

Result<void> EnsureLegacyTemplateFileRemoved(const std::filesystem::path& base_directory) {
    std::error_code ec;
    const auto legacy_path = LegacyTemplateFilePath(base_directory);
    if (!std::filesystem::exists(legacy_path, ec) || ec) {
        return Result<void>::Success();
    }

    return Result<void>::Failure(
        Error(ErrorCode::FILE_IO_ERROR,
              "Legacy template file is no longer supported: " + legacy_path.string(),
              MODULE_NAME));
}

Result<std::vector<Template>> LoadTemplatesFromCanonicalDirectory(
    const std::filesystem::path& templates_directory) {
    std::vector<Template> templates;
    std::error_code ec;
    if (!std::filesystem::exists(templates_directory, ec) || ec) {
        return Result<std::vector<Template>>::Success(templates);
    }

    for (const auto& entry : std::filesystem::directory_iterator(templates_directory, ec)) {
        if (ec) {
            return Result<std::vector<Template>>::Failure(
                Error(ErrorCode::FILE_IO_ERROR,
                      "Failed to iterate templates directory: " + templates_directory.string(),
                      MODULE_NAME));
        }
        if (!entry.is_regular_file()) {
            continue;
        }

        std::ifstream file(entry.path(), std::ios::in | std::ios::binary);
        if (!file.is_open()) {
            return Result<std::vector<Template>>::Failure(
                Error(ErrorCode::FILE_IO_ERROR, "Unable to read file: " + entry.path().string(), MODULE_NAME));
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        auto json_result = RecipeJsonSerializer::Parse(buffer.str());
        if (json_result.IsError()) {
            return Result<std::vector<Template>>::Failure(json_result.GetError());
        }

        auto template_result = RecipeJsonSerializer::TemplateFromJson(json_result.Value());
        if (template_result.IsError()) {
            return Result<std::vector<Template>>::Failure(template_result.GetError());
        }
        templates.push_back(template_result.Value());
    }

    return Result<std::vector<Template>>::Success(templates);
}

struct TemplateInventory {
    std::vector<Template> templates;
};

Result<TemplateInventory> BuildTemplateInventory(
    const std::filesystem::path& base_directory,
    const std::filesystem::path& templates_directory) {
    auto legacy_check_result = EnsureLegacyTemplateFileRemoved(base_directory);
    if (legacy_check_result.IsError()) {
        return Result<TemplateInventory>::Failure(legacy_check_result.GetError());
    }

    TemplateInventory inventory;

    auto canonical_result = LoadTemplatesFromCanonicalDirectory(templates_directory);
    if (canonical_result.IsError()) {
        return Result<TemplateInventory>::Failure(canonical_result.GetError());
    }

    for (const auto& tmpl : canonical_result.Value()) {
        inventory.templates.push_back(tmpl);
    }

    return Result<TemplateInventory>::Success(inventory);
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Recipes {

TemplateFileRepository::TemplateFileRepository(const std::string& base_directory)
    : base_directory_(base_directory),
      templates_directory_(std::filesystem::path(base_directory_) / "templates") {
    auto ensure_result = EnsureDirectory();
    if (ensure_result.IsError()) {
        SILIGEN_LOG_WARNING("Failed to ensure template directory: " + ensure_result.GetError().GetMessage());
    }
}

Result<void> TemplateFileRepository::EnsureDirectory() const {
    std::error_code ec;
    std::filesystem::create_directories(templates_directory_, ec);
    if (ec) {
        return Result<void>::Failure(Error(ErrorCode::FILE_IO_ERROR,
                                           "Failed to create directory: " + templates_directory_.string() + ", " + ec.message(),
                                           MODULE_NAME));
    }
    return Result<void>::Success();
}

Result<void> TemplateFileRepository::WriteJsonFile(const std::filesystem::path& path,
                                                   const nlohmann::json& data) const {
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

Result<nlohmann::json> TemplateFileRepository::ReadJsonFile(const std::filesystem::path& path) const {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<nlohmann::json>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + path.string(), MODULE_NAME));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return RecipeJsonSerializer::Parse(buffer.str());
}

std::filesystem::path TemplateFileRepository::TemplatePath(const TemplateId& template_id) const {
    return templates_directory_ / (template_id + ".json");
}

Result<TemplateId> TemplateFileRepository::SaveTemplate(const Template& template_data) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto ensure_result = EnsureDirectory();
    if (ensure_result.IsError()) {
        return Result<TemplateId>::Failure(ensure_result.GetError());
    }

    Template updated = template_data;
    if (updated.id.empty()) {
        updated.id = GenerateId("template");
    }
    if (updated.created_at == 0) {
        updated.created_at = NowTimestamp();
    }
    updated.updated_at = NowTimestamp();

    auto inventory_result = BuildTemplateInventory(base_directory_, templates_directory_);
    if (inventory_result.IsError()) {
        return Result<TemplateId>::Failure(inventory_result.GetError());
    }

    const auto& inventory = inventory_result.Value();
    for (const auto& existing : inventory.templates) {
        if (ToLowerCopy(existing.name) == ToLowerCopy(updated.name) && existing.id != updated.id) {
            return Result<TemplateId>::Failure(
                Error(ErrorCode::RECIPE_ALREADY_EXISTS,
                      "Template name already exists: " + updated.name,
                      MODULE_NAME));
        }
    }

    auto write_result = WriteJsonFile(TemplatePath(updated.id), RecipeJsonSerializer::TemplateToJson(updated));
    if (write_result.IsError()) {
        return Result<TemplateId>::Failure(write_result.GetError());
    }

    return Result<TemplateId>::Success(updated.id);
}

Result<Template> TemplateFileRepository::GetTemplateById(const TemplateId& template_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto inventory_result = BuildTemplateInventory(base_directory_, templates_directory_);
    if (inventory_result.IsError()) {
        return Result<Template>::Failure(inventory_result.GetError());
    }

    const auto& inventory = inventory_result.Value();
    for (const auto& tmpl : inventory.templates) {
        if (tmpl.id == template_id) {
            return Result<Template>::Success(tmpl);
        }
    }

    return Result<Template>::Failure(
        Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found: " + template_id, MODULE_NAME));
}

Result<Template> TemplateFileRepository::GetTemplateByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto inventory_result = BuildTemplateInventory(base_directory_, templates_directory_);
    if (inventory_result.IsError()) {
        return Result<Template>::Failure(inventory_result.GetError());
    }

    auto name_key = ToLowerCopy(name);
    const auto& inventory = inventory_result.Value();
    for (const auto& tmpl : inventory.templates) {
        if (ToLowerCopy(tmpl.name) == name_key) {
            return Result<Template>::Success(tmpl);
        }
    }

    return Result<Template>::Failure(
        Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found: " + name, MODULE_NAME));
}

Result<std::vector<Template>> TemplateFileRepository::ListTemplates() const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto inventory_result = BuildTemplateInventory(base_directory_, templates_directory_);
    if (inventory_result.IsError()) {
        return Result<std::vector<Template>>::Failure(inventory_result.GetError());
    }

    auto inventory = inventory_result.Value();

    std::sort(inventory.templates.begin(), inventory.templates.end(), [](const Template& a, const Template& b) {
        return ToLowerCopy(a.name) < ToLowerCopy(b.name);
    });

    return Result<std::vector<Template>>::Success(inventory.templates);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
