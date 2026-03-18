#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ContainerBootstrap.h"
#include "application/usecases/recipes/CreateRecipeUseCase.h"
#include "application/usecases/recipes/ExportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/ImportRecipeBundlePayloadUseCase.h"
#include "application/usecases/recipes/RecipeCommandUseCase.h"
#include "application/usecases/recipes/RecipeQueryUseCase.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/recipes/value-objects/RecipeTypes.h"
#include "shared/Strings/StringManipulator.h"
#include "shared/types/Error.h"

#ifndef SILIGEN_GIT_HASH
#define SILIGEN_GIT_HASH "unknown"
#endif

namespace {

using Siligen::Application::Container::ApplicationContainer;
using Siligen::Application::UseCases::Recipes::ArchiveRecipeRequest;
using Siligen::Application::UseCases::Recipes::CreateRecipeRequest;
using Siligen::Application::UseCases::Recipes::ExportRecipeBundlePayloadRequest;
using Siligen::Application::UseCases::Recipes::GetRecipeAuditRequest;
using Siligen::Application::UseCases::Recipes::GetRecipeRequest;
using Siligen::Application::UseCases::Recipes::ImportRecipeBundlePayloadRequest;
using Siligen::Application::UseCases::Recipes::ListRecipesRequest;
using Siligen::Application::UseCases::Recipes::ListRecipeVersionsRequest;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Recipes::ValueObjects::RecipeStatus;
using Siligen::Domain::Recipes::ValueObjects::RecipeVersionStatus;
using Siligen::Shared::Types::Error;
using Siligen::StringManipulator;

struct CliOptions {
    std::string command;
    std::string subcommand;
    std::unordered_map<std::string, std::string> values;
    bool help = false;
    bool version = false;
    bool dry_run = false;
};

std::string Lower(std::string value) {
    return StringManipulator::ToLower(StringManipulator::Trim(value));
}

std::optional<std::string> ConsumeValue(const std::string& arg, const std::string& flag, int argc, char* argv[], int& index) {
    const std::string prefix = flag + "=";
    if (arg == flag) {
        if (index + 1 >= argc) {
            return std::nullopt;
        }
        return std::string(argv[++index]);
    }
    if (arg.rfind(prefix, 0) == 0) {
        return arg.substr(prefix.size());
    }
    return std::nullopt;
}

std::vector<std::string> SplitCsv(const std::string& raw) {
    std::vector<std::string> values;
    std::istringstream stream(raw);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item = StringManipulator::Trim(item);
        if (!item.empty()) {
            values.push_back(item);
        }
    }
    return values;
}

void PrintUsage() {
    std::cout
        << "Siligen CLI canonical 入口\n"
        << "用法:\n"
        << "  siligen_cli bootstrap-check [--config <path>]\n"
        << "  siligen_cli recipe create --name <name> [--description <text>] [--tags a,b] [--actor <name>]\n"
        << "  siligen_cli recipe list [--recipe-query <text>] [--recipe-tag <tag>] [--recipe-status active|archived]\n"
        << "  siligen_cli recipe get --recipe-id <id>\n"
        << "  siligen_cli recipe versions --recipe-id <id>\n"
        << "  siligen_cli recipe audit --recipe-id <id> [--version-id <id>]\n"
        << "  siligen_cli recipe export --recipe-id <id> [--export-file <path>]\n"
        << "  siligen_cli recipe import --import-file <path> [--dry-run] [--resolution rename|skip|replace] [--actor <name>]\n"
        << "\n"
        << "当前 canonical CLI 已承载 bootstrap/recipe 子命令。\n"
        << "运动、点胶、DXF、连接调试命令仍在迁移中；如需 legacy 能力，请显式使用 run.ps1 的 legacy fallback。\n";
}

void PrintVersion() {
    std::cout << "siligen_cli git=" << SILIGEN_GIT_HASH << std::endl;
}

[[noreturn]] void ThrowUsage(const std::string& message) {
    throw std::runtime_error(message);
}

std::string RequireValue(const CliOptions& options, const std::string& key) {
    const auto it = options.values.find(key);
    if (it == options.values.end() || it->second.empty()) {
        ThrowUsage("缺少参数: " + key);
    }
    return it->second;
}

std::string OptionalValue(const CliOptions& options, const std::string& key, const std::string& fallback = "") {
    const auto it = options.values.find(key);
    if (it == options.values.end()) {
        return fallback;
    }
    return it->second;
}

CliOptions ParseArgs(int argc, char* argv[]) {
    CliOptions options;
    if (argc <= 1) {
        options.help = true;
        return options;
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const std::string first = args[1];
    if (first == "-h" || first == "--help") {
        options.help = true;
        return options;
    }
    if (first == "--version" || first == "-v") {
        options.version = true;
        return options;
    }

    options.command = first;
    int option_start = 2;
    if (first == "recipe") {
        if (argc <= 2) {
            ThrowUsage("recipe 需要子命令");
        }
        options.subcommand = args[2];
        option_start = 3;
    }

    for (int i = option_start; i < argc; ++i) {
        const std::string arg = args[static_cast<size_t>(i)];
        if (arg == "--help" || arg == "-h") {
            options.help = true;
            return options;
        }
        if (arg == "--dry-run") {
            options.dry_run = true;
            continue;
        }

        const std::vector<std::string> keys = {
            "--config",
            "--name",
            "--description",
            "--tags",
            "--actor",
            "--recipe-id",
            "--recipe-query",
            "--recipe-tag",
            "--recipe-status",
            "--version-id",
            "--export-file",
            "--import-file",
            "--resolution"
        };

        bool matched = false;
        for (const auto& key : keys) {
            if (auto value = ConsumeValue(arg, key, argc, argv, i)) {
                options.values.emplace(key, *value);
                matched = true;
                break;
            }
        }
        if (matched) {
            continue;
        }

        ThrowUsage("未知参数: " + arg);
    }

    return options;
}

std::shared_ptr<ApplicationContainer> BuildCliContainer(const CliOptions& options) {
    const auto config_path = OptionalValue(options, "--config", "config/machine/machine_config.ini");
    return Siligen::Apps::Runtime::BuildContainer(
        config_path,
        Siligen::Application::Container::LogMode::File,
        "logs/cli_business.log");
}

void PrintError(const Error& error) {
    std::cout << "错误代码: " << static_cast<int>(error.GetCode()) << std::endl;
    std::cout << "错误消息: " << error.GetMessage() << std::endl;
    if (!error.GetModule().empty()) {
        std::cout << "错误模块: " << error.GetModule() << std::endl;
    }
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

int HandleBootstrapCheck(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto config_port = container->ResolvePort<IConfigurationPort>();
    if (!config_port) {
        std::cout << "bootstrap-check failed: IConfigurationPort 未注册" << std::endl;
        return 1;
    }

    auto hardware_mode = config_port->GetHardwareMode();
    if (hardware_mode.IsError()) {
        PrintError(hardware_mode.GetError());
        return 1;
    }

    std::cout << "bootstrap-check passed" << std::endl;
    std::cout << "hardware-mode: " << static_cast<int>(hardware_mode.Value()) << std::endl;
    return 0;
}

int HandleRecipeCreate(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::CreateRecipeUseCase>();
    if (!usecase) {
        std::cout << "错误: 配方创建用例不可用" << std::endl;
        return 1;
    }

    CreateRecipeRequest request;
    request.name = RequireValue(options, "--name");
    request.description = OptionalValue(options, "--description");
    request.tags = SplitCsv(OptionalValue(options, "--tags"));
    request.actor = OptionalValue(options, "--actor", "cli");

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& recipe = result.Value().recipe;
    std::cout << "已创建配方: " << recipe.id << " (" << recipe.name << ")" << std::endl;
    return 0;
}

int HandleRecipeList(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 配方列表用例不可用" << std::endl;
        return 1;
    }

    ListRecipesRequest request;
    request.query = OptionalValue(options, "--recipe-query");
    request.tag = OptionalValue(options, "--recipe-tag");

    const auto status = Lower(OptionalValue(options, "--recipe-status"));
    if (!status.empty()) {
        if (status != "active" && status != "archived") {
            ThrowUsage("recipe-status 仅支持 active|archived");
        }
        request.status = status;
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

int HandleRecipeGet(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 获取配方用例不可用" << std::endl;
        return 1;
    }

    GetRecipeRequest request;
    request.recipe_id = RequireValue(options, "--recipe-id");

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    const auto& recipe = result.Value().recipe;
    std::cout << "配方ID: " << recipe.id << std::endl;
    std::cout << "名称: " << recipe.name << std::endl;
    std::cout << "状态: " << RecipeStatusToString(recipe.status) << std::endl;
    std::cout << "当前版本: " << recipe.active_version_id << std::endl;
    return 0;
}

int HandleRecipeVersions(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 获取版本列表用例不可用" << std::endl;
        return 1;
    }

    ListRecipeVersionsRequest request;
    request.recipe_id = RequireValue(options, "--recipe-id");

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

int HandleRecipeAudit(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::RecipeQueryUseCase>();
    if (!usecase) {
        std::cout << "错误: 审计查询用例不可用" << std::endl;
        return 1;
    }

    GetRecipeAuditRequest request;
    request.recipe_id = RequireValue(options, "--recipe-id");
    request.version_id = OptionalValue(options, "--version-id");

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    for (const auto& record : result.Value().records) {
        std::cout << "- " << record.id << " | " << record.actor << " | " << record.timestamp << std::endl;
    }
    return 0;
}

int HandleRecipeExport(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::ExportRecipeBundlePayloadUseCase>();
    if (!usecase) {
        std::cout << "错误: 导出用例不可用" << std::endl;
        return 1;
    }

    const auto recipe_id = RequireValue(options, "--recipe-id");
    const auto output_path = OptionalValue(options, "--export-file", "recipe_export_" + recipe_id + ".json");

    ExportRecipeBundlePayloadRequest request;
    request.recipe_id = recipe_id;

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        std::cout << "错误: 无法写入导出文件 " << output_path << std::endl;
        return 1;
    }

    output << result.Value().bundle_json;
    if (!output) {
        std::cout << "错误: 导出文件写入失败" << std::endl;
        return 1;
    }

    std::cout << "已导出配方到: " << output_path << std::endl;
    return 0;
}

int HandleRecipeImport(const CliOptions& options) {
    auto container = BuildCliContainer(options);
    auto usecase = container->Resolve<Siligen::Application::UseCases::Recipes::ImportRecipeBundlePayloadUseCase>();
    if (!usecase) {
        std::cout << "错误: 导入用例不可用" << std::endl;
        return 1;
    }

    const auto input_path = RequireValue(options, "--import-file");
    std::ifstream input(input_path, std::ios::binary);
    if (!input.is_open()) {
        std::cout << "错误: 无法读取导入文件: " << input_path << std::endl;
        return 1;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    const auto resolution = Lower(OptionalValue(options, "--resolution"));
    if (!resolution.empty() && resolution != "rename" && resolution != "skip" && resolution != "replace") {
        ThrowUsage("resolution 仅支持 rename|skip|replace");
    }

    ImportRecipeBundlePayloadRequest request;
    request.bundle_json = buffer.str();
    request.dry_run = options.dry_run;
    request.actor = OptionalValue(options, "--actor", "cli");
    request.source_label = input_path;
    request.resolution_strategy = resolution;

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

int HandleUnsupportedCommand(const CliOptions& options) {
    std::ostringstream message;
    message << "命令尚未迁移到 canonical CLI: " << options.command;
    if (!options.subcommand.empty()) {
        message << " " << options.subcommand;
    }
    message << "\n当前 canonical CLI 已承载: bootstrap-check, recipe create/list/get/versions/audit/export/import"
            << "\n如需 legacy CLI，请显式执行 apps/control-cli/run.ps1 -UseLegacyFallback -- <legacy-args>";
    std::cout << message.str() << std::endl;
    return 3;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = ParseArgs(argc, argv);
        if (options.help) {
            PrintUsage();
            return 0;
        }
        if (options.version) {
            PrintVersion();
            return 0;
        }

        if (options.command == "bootstrap-check") {
            return HandleBootstrapCheck(options);
        }

        if (options.command != "recipe") {
            return HandleUnsupportedCommand(options);
        }

        if (options.subcommand == "create") {
            return HandleRecipeCreate(options);
        }
        if (options.subcommand == "list") {
            return HandleRecipeList(options);
        }
        if (options.subcommand == "get") {
            return HandleRecipeGet(options);
        }
        if (options.subcommand == "versions") {
            return HandleRecipeVersions(options);
        }
        if (options.subcommand == "audit") {
            return HandleRecipeAudit(options);
        }
        if (options.subcommand == "export") {
            return HandleRecipeExport(options);
        }
        if (options.subcommand == "import") {
            return HandleRecipeImport(options);
        }

        return HandleUnsupportedCommand(options);
    } catch (const std::exception& ex) {
        std::cerr << "[control-cli] 错误: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[control-cli] 未知错误" << std::endl;
        return 2;
    }
}
