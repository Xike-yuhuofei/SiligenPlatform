#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

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
#include "recipe_lifecycle/domain/recipes/value-objects/RecipeTypes.h"

#include <fstream>
#include <sstream>
#include <variant>

namespace {

using Siligen::Domain::Recipes::ValueObjects::ConflictResolution;
using Siligen::Domain::Recipes::ValueObjects::ParameterValue;
using Siligen::Domain::Recipes::ValueObjects::ParameterValueEntry;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::StringManipulator;

bool ParseBoolValue(const std::string& value, bool& out) {
    if (value == "true" || value == "1" || value == "yes" || value == "on") {
        out = true;
        return true;
    }
    if (value == "false" || value == "0" || value == "no" || value == "off") {
        out = false;
        return true;
    }
    return false;
}

bool ParseInt64Value(const std::string& value, std::int64_t& out) {
    std::istringstream iss(value);
    iss >> out;
    return !iss.fail() && iss.eof();
}

bool ParseDoubleValue(const std::string& value, double& out) {
    std::istringstream iss(value);
    iss >> out;
    return !iss.fail() && iss.eof();
}

bool ParseParameterToken(const std::string& token, ParameterValueEntry& entry) {
    const auto eq_pos = token.find('=');
    if (eq_pos == std::string::npos) {
        return false;
    }

    std::string left = token.substr(0, eq_pos);
    const std::string value = token.substr(eq_pos + 1);
    if (left.empty() || value.empty()) {
        return false;
    }

    std::string key = left;
    std::string type_hint;
    const auto colon_pos = left.find(':');
    if (colon_pos != std::string::npos) {
        key = left.substr(0, colon_pos);
        type_hint = left.substr(colon_pos + 1);
    }

    key = StringManipulator::Trim(key);
    type_hint = StringManipulator::ToLower(StringManipulator::Trim(type_hint));
    if (key.empty()) {
        return false;
    }

    entry.key = key;
    if (type_hint == "int") {
        std::int64_t number = 0;
        if (!ParseInt64Value(value, number)) {
            return false;
        }
        entry.value = number;
        return true;
    }
    if (type_hint == "float") {
        double number = 0.0;
        if (!ParseDoubleValue(value, number)) {
            return false;
        }
        entry.value = number;
        return true;
    }
    if (type_hint == "bool") {
        bool flag = false;
        if (!ParseBoolValue(StringManipulator::ToLower(value), flag)) {
            return false;
        }
        entry.value = flag;
        return true;
    }
    if (type_hint == "string" || type_hint == "enum") {
        entry.value = value;
        return true;
    }

    bool flag = false;
    if (ParseBoolValue(StringManipulator::ToLower(value), flag)) {
        entry.value = flag;
        return true;
    }

    std::int64_t int_value = 0;
    if (ParseInt64Value(value, int_value)) {
        entry.value = int_value;
        return true;
    }

    double float_value = 0.0;
    if (ParseDoubleValue(value, float_value)) {
        entry.value = float_value;
        return true;
    }

    entry.value = value;
    return true;
}

Result<RecipeStatus> ParseRecipeStatus(const std::string& value) {
    const auto lowered = StringManipulator::ToLower(value);
    if (lowered == "active") {
        return Result<RecipeStatus>::Success(RecipeStatus::Active);
    }
    if (lowered == "archived") {
        return Result<RecipeStatus>::Success(RecipeStatus::Archived);
    }
    return Result<RecipeStatus>::Failure(
        Error(ErrorCode::INVALID_PARAMETER, "未知配方状态", "CLI"));
}

Result<ConflictResolution> ParseConflictResolution(const std::string& value) {
    const auto lowered = StringManipulator::ToLower(value);
    if (lowered == "rename") {
        return Result<ConflictResolution>::Success(ConflictResolution::Rename);
    }
    if (lowered == "skip") {
        return Result<ConflictResolution>::Success(ConflictResolution::Skip);
    }
    if (lowered == "replace") {
        return Result<ConflictResolution>::Success(ConflictResolution::Replace);
    }
    return Result<ConflictResolution>::Failure(
        Error(ErrorCode::INVALID_PARAMETER, "未知冲突策略", "CLI"));
}

std::string RecipeStatusToString(RecipeStatus status) {
    return status == RecipeStatus::Archived ? "archived" : "active";
}

std::string VersionStatusToString(RecipeVersionStatus status) {
    switch (status) {
        case RecipeVersionStatus::Draft:
            return "draft";
        case RecipeVersionStatus::Published:
            return "published";
        case RecipeVersionStatus::Archived:
            return "archived";
    }
    return "draft";
}

std::string ParameterValueToString(const ParameterValue& value) {
    return std::visit(
        [](const auto& v) {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        },
        value);
}

}  // namespace

namespace Siligen::Adapters::CLI {

int CLICommandHandlers::HandleRecipeCreate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_name.empty()) {
        std::cout << "错误: 缺少配方名称 (--name)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::CreateRecipeUseCase>();
    if (!usecase) {
        std::cout << "错误: 配方创建用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::CreateRecipeRequest request;
    request.name = config.recipe_name;
    request.description = config.recipe_description;
    request.tags = config.recipe_tags;
    request.actor = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& recipe = result.Value().recipe;
    std::cout << "已创建配方: " << recipe.id << " (" << recipe.name << ")" << std::endl;

    if (config.template_id.empty()) {
        return 0;
    }

    auto draft_usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::CreateDraftVersionUseCase>();
    if (!draft_usecase) {
        std::cout << "警告: 创建草稿版本用例不可用" << std::endl;
        return 0;
    }

    Siligen::Application::UseCases::Recipes::CreateDraftVersionRequest draft_request;
    draft_request.recipe_id = recipe.id;
    draft_request.template_id = config.template_id;
    draft_request.base_version_id = config.base_version_id;
    draft_request.version_label = config.version_label;
    draft_request.change_note = config.change_note;
    draft_request.created_by = config.actor;

    auto draft_result = draft_usecase->Execute(draft_request);
    if (draft_result.IsError()) {
        PrintError(draft_result.GetError());
        return 1;
    }

    std::cout << "已创建草稿版本: " << draft_result.Value().version.id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeUpdate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::UpdateRecipeUseCase>();
    if (!usecase) {
        std::cout << "错误: 配方更新用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::UpdateRecipeRequest request;
    request.recipe_id = config.recipe_id;
    request.actor = config.actor;
    if (!config.recipe_name.empty()) {
        request.name = config.recipe_name;
        request.update_name = true;
    }
    if (!config.recipe_description.empty()) {
        request.description = config.recipe_description;
        request.update_description = true;
    }
    if (!config.recipe_tags.empty()) {
        request.tags = config.recipe_tags;
        request.update_tags = true;
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& recipe = result.Value().recipe;
    std::cout << "已更新配方: " << recipe.id << " (" << recipe.name << ")" << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeDraftCreate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::CreateDraftVersionUseCase>();
    if (!usecase) {
        std::cout << "错误: 创建草稿版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::CreateDraftVersionRequest request;
    request.recipe_id = config.recipe_id;
    request.template_id = config.template_id;
    request.base_version_id = config.base_version_id;
    request.version_label = config.version_label;
    request.change_note = config.change_note;
    request.created_by = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已创建草稿版本: " << result.Value().version.id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeDraftUpdate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty() || config.version_id.empty()) {
        std::cout << "错误: 缺少配方 ID 或版本 ID (--recipe-id, --version-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::UpdateDraftVersionUseCase>();
    if (!usecase) {
        std::cout << "错误: 更新草稿版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::UpdateDraftVersionRequest request;
    request.recipe_id = config.recipe_id;
    request.version_id = config.version_id;
    request.editor = config.actor;
    request.change_note = config.change_note;

    if (!config.recipe_params.empty()) {
        for (const auto& token : config.recipe_params) {
            ParameterValueEntry entry;
            if (!ParseParameterToken(token, entry)) {
                std::cout << "错误: 参数格式无效: " << token << std::endl;
                return 1;
            }
            request.parameters.push_back(entry);
        }
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已更新草稿版本: " << result.Value().version.id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipePublish(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty() || config.version_id.empty()) {
        std::cout << "错误: 缺少配方 ID 或版本 ID (--recipe-id, --version-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeCommandUseCase>();
    if (!usecase) {
        std::cout << "错误: 发布版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::PublishRecipeVersionRequest request;
    request.recipe_id = config.recipe_id;
    request.version_id = config.version_id;
    request.actor = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已发布版本: " << result.Value().version.id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeList(const Siligen::Application::CommandLineConfig& config) {
    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 配方列表用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::ListRecipesRequest request;
    request.query = config.recipe_query;
    request.tag = config.recipe_tag;
    if (!config.recipe_status.empty()) {
        auto status_result = ParseRecipeStatus(config.recipe_status);
        if (status_result.IsError()) {
            std::cout << "错误: 状态无效 (active/archived)" << std::endl;
            return 1;
        }
        request.status = RecipeStatusToString(status_result.Value());
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    for (const auto& recipe : result.Value().recipes) {
        std::cout << "- " << recipe.id << " | " << recipe.name << " | "
                  << RecipeStatusToString(recipe.status) << " | active=" << recipe.active_version_id << std::endl;
    }
    return 0;
}

int CLICommandHandlers::HandleRecipeGet(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 获取配方用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::GetRecipeRequest request;
    request.recipe_id = config.recipe_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& recipe = result.Value().recipe;
    std::cout << "配方 ID: " << recipe.id << std::endl;
    std::cout << "名称: " << recipe.name << std::endl;
    std::cout << "状态: " << RecipeStatusToString(recipe.status) << std::endl;
    std::cout << "当前版本: " << recipe.active_version_id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeListVersions(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 获取版本列表用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::ListRecipeVersionsRequest request;
    request.recipe_id = config.recipe_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    for (const auto& version : result.Value().versions) {
        std::cout << "- " << version.id << " | " << version.version_label << " | "
                  << VersionStatusToString(version.status) << std::endl;
    }
    return 0;
}

int CLICommandHandlers::HandleRecipeArchive(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeCommandUseCase>();
    if (!usecase) {
        std::cout << "错误: 归档配方用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::ArchiveRecipeRequest request;
    request.recipe_id = config.recipe_id;
    request.actor = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << (result.Value().archived ? "已归档配方: " : "配方未变更: ") << config.recipe_id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeVersionCreate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::CreateVersionFromPublishedUseCase>();
    if (!usecase) {
        std::cout << "错误: 创建版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::CreateVersionFromPublishedRequest request;
    request.recipe_id = config.recipe_id;
    request.base_version_id = config.base_version_id;
    request.version_label = config.version_label;
    request.change_note = config.change_note;
    request.created_by = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已创建新版本草稿: " << result.Value().version.id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeCompare(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty() || config.version_id.empty() || config.base_version_id.empty()) {
        std::cout << "错误: 缺少配方 ID 或版本 ID (--recipe-id, --version-id, --base-version-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::CompareRecipeVersionsUseCase>();
    if (!usecase) {
        std::cout << "错误: 比较版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::CompareRecipeVersionsRequest request;
    request.recipe_id = config.recipe_id;
    request.from_version_id = config.base_version_id;
    request.to_version_id = config.version_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    for (const auto& change : result.Value().changes) {
        std::cout << "- " << change.field << ": " << change.before << " -> " << change.after << std::endl;
    }
    return 0;
}

int CLICommandHandlers::HandleRecipeActivate(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty() || config.version_id.empty()) {
        std::cout << "错误: 缺少配方 ID 或版本 ID (--recipe-id, --version-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeCommandUseCase>();
    if (!usecase) {
        std::cout << "错误: 激活版本用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::ActivateRecipeVersionRequest request;
    request.recipe_id = config.recipe_id;
    request.version_id = config.version_id;
    request.actor = config.actor;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已激活版本: " << result.Value().active_version_id << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeAudit(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 审计查询用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::GetRecipeAuditRequest request;
    request.recipe_id = config.recipe_id;
    request.version_id = config.version_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    for (const auto& record : result.Value().records) {
        std::cout << "- " << record.id << " | " << record.actor << " | " << record.timestamp;
        if (record.version_id.has_value()) {
            std::cout << " | version=" << record.version_id.value();
        }
        if (!record.changes.empty()) {
            std::cout << " | changes=";
            bool first = true;
            for (const auto& change : record.changes) {
                if (!first) {
                    std::cout << "; ";
                }
                first = false;
                std::cout << change.field << ":" << change.before << "->" << change.after;
            }
        }
        std::cout << std::endl;
    }
    return 0;
}

int CLICommandHandlers::HandleRecipeExport(const Siligen::Application::CommandLineConfig& config) {
    if (config.recipe_id.empty()) {
        std::cout << "错误: 缺少配方 ID (--recipe-id)" << std::endl;
        return 1;
    }

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::ExportRecipeBundlePayloadUseCase>();
    if (!usecase) {
        std::cout << "错误: 导出用例不可用" << std::endl;
        return 1;
    }

    const std::string output_path = config.export_path.empty()
        ? ("recipe_export_" + config.recipe_id + ".json")
        : config.export_path;

    Siligen::Application::UseCases::Recipes::ExportRecipeBundlePayloadRequest request;
    request.recipe_id = config.recipe_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        PrintError(Error(ErrorCode::FILE_IO_ERROR, "无法写入导出文件", "CLI"));
        return 1;
    }

    out << result.Value().bundle_json;
    if (!out) {
        PrintError(Error(ErrorCode::FILE_IO_ERROR, "导出文件写入失败", "CLI"));
        return 1;
    }

    std::cout << "已导出配方到: " << output_path << std::endl;
    return 0;
}

int CLICommandHandlers::HandleRecipeImport(const Siligen::Application::CommandLineConfig& config) {
    if (config.import_path.empty()) {
        std::cout << "错误: 缺少导入文件路径 (--import-file)" << std::endl;
        return 1;
    }

    std::ifstream file(config.import_path, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "错误: 无法读取导入文件: " << config.import_path << std::endl;
        return 1;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    auto usecase = container_->Resolve<Siligen::Application::UseCases::Recipes::ImportRecipeBundlePayloadUseCase>();
    if (!usecase) {
        std::cout << "错误: 导入用例不可用" << std::endl;
        return 1;
    }

    Siligen::Application::UseCases::Recipes::ImportRecipeBundlePayloadRequest request;
    request.bundle_json = buffer.str();
    request.dry_run = config.dry_run;
    request.actor = config.actor;
    request.source_label = config.import_path;

    if (!config.conflict_resolution.empty()) {
        auto resolution = ParseConflictResolution(config.conflict_resolution);
        if (resolution.IsError()) {
            std::cout << "错误: 冲突策略无效 (rename/skip/replace)" << std::endl;
            return 1;
        }
        request.resolution_strategy = StringManipulator::ToLower(config.conflict_resolution);
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    if (result.Value().status == "conflicts") {
        std::cout << "检测到冲突:" << std::endl;
        for (const auto& conflict : result.Value().conflicts) {
            std::cout << "- " << conflict.message << std::endl;
        }
        return 2;
    }

    std::cout << "导入完成，处理配方数: " << result.Value().imported_count << std::endl;
    return 0;
}

}  // namespace Siligen::Adapters::CLI
