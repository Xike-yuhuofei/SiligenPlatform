#include "TemplateFileRepository.h"

#include "RecipeJsonSerializer.h"
#include "shared/types/Error.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <unordered_map>

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

Result<std::vector<Template>> LoadTemplatesFromLegacyFile(const std::filesystem::path& path) {
    std::vector<Template> templates;
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return Result<std::vector<Template>>::Success(templates);
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return Result<std::vector<Template>>::Failure(
            Error(ErrorCode::FILE_IO_ERROR, "Unable to read file: " + path.string(), MODULE_NAME));
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    auto json_result = RecipeJsonSerializer::Parse(buffer.str());
    if (json_result.IsError()) {
        return Result<std::vector<Template>>::Failure(json_result.GetError());
    }

    const auto& root = json_result.Value();
    if (root.contains("templates") && root.at("templates").is_array()) {
        for (const auto& item : root.at("templates")) {
            auto template_result = RecipeJsonSerializer::TemplateFromJson(item);
            if (template_result.IsError()) {
                return Result<std::vector<Template>>::Failure(template_result.GetError());
            }
            templates.push_back(template_result.Value());
        }
    } else if (root.is_object() && root.contains("id")) {
        auto template_result = RecipeJsonSerializer::TemplateFromJson(root);
        if (template_result.IsError()) {
            return Result<std::vector<Template>>::Failure(template_result.GetError());
        }
        templates.push_back(template_result.Value());
    }

    return Result<std::vector<Template>>::Success(templates);
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

    std::error_code ec;
    if (std::filesystem::exists(templates_directory_, ec) && !ec) {
        for (const auto& entry : std::filesystem::directory_iterator(templates_directory_, ec)) {
            if (ec) {
                return Result<TemplateId>::Failure(
                    Error(ErrorCode::FILE_IO_ERROR,
                          "Failed to iterate templates directory: " + templates_directory_.string(),
                          MODULE_NAME));
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            auto json_result = ReadJsonFile(entry.path());
            if (json_result.IsError()) {
                return Result<TemplateId>::Failure(json_result.GetError());
            }
            auto template_result = RecipeJsonSerializer::TemplateFromJson(json_result.Value());
            if (template_result.IsError()) {
                return Result<TemplateId>::Failure(template_result.GetError());
            }
            const auto& existing = template_result.Value();
            if (ToLowerCopy(existing.name) == ToLowerCopy(updated.name) && existing.id != updated.id) {
                return Result<TemplateId>::Failure(
                    Error(ErrorCode::RECIPE_ALREADY_EXISTS,
                          "Template name already exists: " + updated.name,
                          MODULE_NAME));
            }
        }
    }

    auto legacy_path = base_directory_ / "templates.json";
    auto legacy_result = LoadTemplatesFromLegacyFile(legacy_path);
    if (legacy_result.IsError()) {
        return Result<TemplateId>::Failure(legacy_result.GetError());
    }
    for (const auto& existing : legacy_result.Value()) {
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

    auto path = TemplatePath(template_id);
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec) {
        auto json_result = ReadJsonFile(path);
        if (json_result.IsError()) {
            return Result<Template>::Failure(json_result.GetError());
        }

        return RecipeJsonSerializer::TemplateFromJson(json_result.Value());
    }

    auto legacy_path = base_directory_ / "templates.json";
    auto legacy_result = LoadTemplatesFromLegacyFile(legacy_path);
    if (legacy_result.IsError()) {
        return Result<Template>::Failure(legacy_result.GetError());
    }

    for (const auto& tmpl : legacy_result.Value()) {
        if (tmpl.id == template_id) {
            return Result<Template>::Success(tmpl);
        }
    }

    return Result<Template>::Failure(
        Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found: " + template_id, MODULE_NAME));
}

Result<Template> TemplateFileRepository::GetTemplateByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::error_code ec;
    if (!std::filesystem::exists(templates_directory_, ec) || ec) {
        return Result<Template>::Failure(
            Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found: " + name, MODULE_NAME));
    }

    auto name_key = ToLowerCopy(name);
    for (const auto& entry : std::filesystem::directory_iterator(templates_directory_, ec)) {
        if (ec) {
            return Result<Template>::Failure(
                Error(ErrorCode::FILE_IO_ERROR,
                      "Failed to iterate templates directory: " + templates_directory_.string(),
                      MODULE_NAME));
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        auto json_result = ReadJsonFile(entry.path());
        if (json_result.IsError()) {
            return Result<Template>::Failure(json_result.GetError());
        }
        auto template_result = RecipeJsonSerializer::TemplateFromJson(json_result.Value());
        if (template_result.IsError()) {
            return Result<Template>::Failure(template_result.GetError());
        }
        const auto& tmpl = template_result.Value();
        if (ToLowerCopy(tmpl.name) == name_key) {
            return Result<Template>::Success(tmpl);
        }
    }

    auto legacy_path = base_directory_ / "templates.json";
    auto legacy_result = LoadTemplatesFromLegacyFile(legacy_path);
    if (legacy_result.IsError()) {
        return Result<Template>::Failure(legacy_result.GetError());
    }
    for (const auto& tmpl : legacy_result.Value()) {
        if (ToLowerCopy(tmpl.name) == name_key) {
            return Result<Template>::Success(tmpl);
        }
    }

    return Result<Template>::Failure(
        Error(ErrorCode::TEMPLATE_NOT_FOUND, "Template not found: " + name, MODULE_NAME));
}

Result<std::vector<Template>> TemplateFileRepository::ListTemplates() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::unordered_map<std::string, Template> merged;
    std::error_code ec;
    if (std::filesystem::exists(templates_directory_, ec) && !ec) {
        for (const auto& entry : std::filesystem::directory_iterator(templates_directory_, ec)) {
            if (ec) {
                return Result<std::vector<Template>>::Failure(
                    Error(ErrorCode::FILE_IO_ERROR,
                          "Failed to iterate templates directory: " + templates_directory_.string(),
                          MODULE_NAME));
            }
            if (!entry.is_regular_file()) {
                continue;
            }
            auto json_result = ReadJsonFile(entry.path());
            if (json_result.IsError()) {
                return Result<std::vector<Template>>::Failure(json_result.GetError());
            }
            auto template_result = RecipeJsonSerializer::TemplateFromJson(json_result.Value());
            if (template_result.IsError()) {
                return Result<std::vector<Template>>::Failure(template_result.GetError());
            }
            auto tmpl = template_result.Value();
            merged.emplace(tmpl.id, tmpl);
        }
    }

    auto legacy_path = base_directory_ / "templates.json";
    auto legacy_result = LoadTemplatesFromLegacyFile(legacy_path);
    if (legacy_result.IsError()) {
        return Result<std::vector<Template>>::Failure(legacy_result.GetError());
    }
    for (const auto& tmpl : legacy_result.Value()) {
        merged.emplace(tmpl.id, tmpl);
    }

    std::vector<Template> templates;
    templates.reserve(merged.size());
    for (const auto& pair : merged) {
        templates.push_back(pair.second);
    }

    std::sort(templates.begin(), templates.end(), [](const Template& a, const Template& b) {
        return ToLowerCopy(a.name) < ToLowerCopy(b.name);
    });

    return Result<std::vector<Template>>::Success(templates);
}

}  // namespace Siligen::Infrastructure::Adapters::Recipes
