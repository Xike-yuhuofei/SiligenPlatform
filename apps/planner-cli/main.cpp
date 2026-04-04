#include <iostream>
#include <memory>
#include <string>

#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"
#include "CommandLineParser.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_process_bootstrap/ContainerBootstrap.h"
#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"
#include "shared/types/Error.h"

#ifndef SILIGEN_GIT_HASH
#define SILIGEN_GIT_HASH "unknown"
#endif

namespace {

using Siligen::Adapters::CLI::CLICommandHandlers;
using Siligen::Application::CommandLineConfig;
using Siligen::Application::CommandLineParser;
using Siligen::Application::CommandType;
using Siligen::Application::Container::ApplicationContainer;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Shared::Types::Error;

void PrintUsage() {
    std::cout
        << "Siligen CLI canonical 入口\n"
        << "用法:\n"
        << "  siligen_planner_cli bootstrap-check [--config <path>]\n"
        << "  siligen_planner_cli connect|disconnect|status\n"
        << "  siligen_planner_cli home [--axis <n>|--all] [--timeout-ms <ms>]\n"
        << "  siligen_planner_cli jog --axis <n> --direction <1|-1> --distance <mm> [--velocity <mm/s>]\n"
        << "  siligen_planner_cli move --axis <n> --position <mm> [--relative] [--velocity <mm/s>] [--acceleration <mm/s^2>]\n"
        << "  siligen_planner_cli stop-all [--immediate]\n"
        << "  siligen_planner_cli estop\n"
        << "  siligen_planner_cli dispenser start|purge|stop|pause|resume [...]\n"
        << "  siligen_planner_cli supply open|close\n"
        << "  siligen_planner_cli dxf-plan --file <path> [...]\n"
        << "  siligen_planner_cli dxf-dispense --file <path> [...]\n"
        << "  siligen_planner_cli dxf-preview --file <path> [--output-dir <dir>] [--preview-max-points <n>] [--json]\n"
        << "  siligen_planner_cli dxf-preview-snapshot --file <path> [--dxf-speed <mm/s>] [--dry-run] [--json]\n"
        << "  siligen_planner_cli dxf-augment --file <path> [--output <path>] [--dxf-r12]\n"
        << "  siligen_planner_cli recipe create|update|draft|draft-update|publish [...]\n"
        << "  siligen_planner_cli recipe list|get|versions|archive|version-create|compare|rollback|activate|audit|export|import [...]\n"
        << "\n"
        << "说明:\n"
        << "  - canonical CLI 已承载连接调试、运动、点胶、DXF 与完整 recipe 命令面\n"
        << "  - 默认配置优先使用 "
        << Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath
        << "，可通过 --config 覆盖\n"
        << "  - 不再依赖 legacy build/bin CLI 产物或旧 wrapper fallback\n";
}

void PrintVersion() {
    std::cout << "siligen_planner_cli git=" << SILIGEN_GIT_HASH << std::endl;
}

void PrintError(const Error& error) {
    std::cout << "错误代码: " << static_cast<int>(error.GetCode()) << std::endl;
    std::cout << "错误消息: " << error.GetMessage() << std::endl;
    if (!error.GetModule().empty()) {
        std::cout << "错误模块: " << error.GetModule() << std::endl;
    }
}

std::shared_ptr<ApplicationContainer> BuildCliContainer(const CommandLineConfig& config) {
    return Siligen::Apps::Runtime::BuildContainer(
        ResolveCliConfigPath(config.config_path),
        Siligen::Application::Container::LogMode::File,
        "logs/cli_business.log");
}

int HandleBootstrapCheck(const CommandLineConfig& config) {
    auto container = BuildCliContainer(config);
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

}  // namespace

int main(int argc, char* argv[]) {
    int exit_code = 0;

    try {
        auto config = CommandLineParser::Parse(argc, argv);
        if (config.help) {
            PrintUsage();
            return 0;
        }
        if (config.version) {
            PrintVersion();
            return 0;
        }

        if (config.command == CommandType::BOOTSTRAP_CHECK) {
            return HandleBootstrapCheck(config);
        }

        auto container = BuildCliContainer(config);
        CLICommandHandlers handlers(container);

        if (config.no_interactive && config.command == CommandType::INTERACTIVE) {
            std::cout << "未指定命令且已禁用交互模式，请查看 --help" << std::endl;
            return 1;
        }

        if (config.no_interactive || config.command != CommandType::INTERACTIVE) {
            exit_code = handlers.Execute(config);
        } else {
            exit_code = handlers.RunInteractive();
        }
    } catch (const std::exception& e) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "程序异常" << std::endl;
        std::cerr << "========================================" << std::endl;
        std::cerr << "错误: " << e.what() << std::endl;
        std::cerr << "========================================" << std::endl;
        exit_code = 1;
    } catch (...) {
        std::cerr << "\n========================================" << std::endl;
        std::cerr << "程序发生未知异常" << std::endl;
        std::cerr << "========================================" << std::endl;
        exit_code = 2;
    }

    return exit_code;
}
