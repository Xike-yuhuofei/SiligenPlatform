#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

#include "runtime_process_bootstrap/ContainerBootstrap.h"
#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"

#ifndef SILIGEN_GIT_HASH
#define SILIGEN_GIT_HASH "unknown"
#endif

namespace {

struct RuntimeOptions {
    std::string config_path = Siligen::Infrastructure::Configuration::CanonicalMachineConfigRelativePath();
    std::string log_path = "logs/control_runtime.log";
    int task_threads = 4;
    int run_seconds = 0;
    bool console_log = false;
    bool help = false;
    bool version = false;
};

std::atomic_bool g_keep_running{true};
std::condition_variable g_shutdown_cv;
std::mutex g_shutdown_mutex;

void SignalHandler(int signal) {
    std::cout << "\n[runtime-service] 收到信号 " << signal << "，准备退出..." << std::endl;
    g_keep_running.store(false);
    g_shutdown_cv.notify_all();
}

void PrintUsage() {
    std::cout
        << "用法: siligen_runtime_service [选项]\n"
        << "选项:\n"
        << "  -c, --config <路径>      机器配置文件路径 (默认: "
        << Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath << ")\n"
        << "  -l, --log <路径>         日志文件路径 (默认: logs/control_runtime.log)\n"
        << "  -t, --threads <数量>     任务调度线程数 (默认: 4)\n"
        << "  --console-log            输出日志到控制台\n"
        << "  --run-seconds <秒>       运行指定秒数后退出 (默认: 0，直到 Ctrl+C)\n"
        << "  --version                显示版本\n"
        << "  -h, --help               显示帮助\n";
}

RuntimeOptions ParseArgs(int argc, char* argv[]) {
    RuntimeOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--help") || (arg == "-h")) {
            options.help = true;
            return options;
        }
        if (arg == "--version") {
            options.version = true;
            return options;
        }
        if ((arg == "--config") || (arg == "-c")) {
            if (i + 1 >= argc) {
                throw std::runtime_error("缺少 --config 参数值");
            }
            options.config_path = argv[++i];
            continue;
        }
        if ((arg == "--log") || (arg == "-l")) {
            if (i + 1 >= argc) {
                throw std::runtime_error("缺少 --log 参数值");
            }
            options.log_path = argv[++i];
            continue;
        }
        if ((arg == "--threads") || (arg == "-t")) {
            if (i + 1 >= argc) {
                throw std::runtime_error("缺少 --threads 参数值");
            }
            options.task_threads = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--run-seconds") {
            if (i + 1 >= argc) {
                throw std::runtime_error("缺少 --run-seconds 参数值");
            }
            options.run_seconds = std::stoi(argv[++i]);
            continue;
        }
        if (arg == "--console-log") {
            options.console_log = true;
            continue;
        }

        throw std::runtime_error("未知参数: " + arg);
    }

    if (options.task_threads < 1) {
        throw std::runtime_error("任务线程数必须大于 0");
    }
    if (options.run_seconds < 0) {
        throw std::runtime_error("运行时长不能为负数");
    }

    return options;
}

void EnsureParentDirectory(const std::string& file_path) {
    std::filesystem::path path(file_path);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        auto options = ParseArgs(argc, argv);
        if (options.help) {
            PrintUsage();
            return 0;
        }
        if (options.version) {
            std::cout << "siligen_runtime_service git=" << SILIGEN_GIT_HASH << std::endl;
            return 0;
        }

        EnsureParentDirectory(options.log_path);

        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        const auto log_mode = options.console_log
            ? Siligen::Application::Container::LogMode::Console
            : Siligen::Application::Container::LogMode::File;

        std::string resolved_config_path;
        auto container = Siligen::Apps::Runtime::BuildContainer(
            options.config_path,
            log_mode,
            options.log_path,
            options.task_threads,
            &resolved_config_path
        );

        std::cout << "[runtime-service] 已完成宿主装配" << std::endl;
        std::cout << "[runtime-service] config=" << resolved_config_path << std::endl;
        std::cout << "[runtime-service] log=" << options.log_path << std::endl;
        std::cout << "[runtime-service] task_threads=" << options.task_threads << std::endl;
        std::cout << "[runtime-service] git=" << SILIGEN_GIT_HASH << std::endl;

        if (options.run_seconds > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(options.run_seconds));
            std::cout << "[runtime-service] 已按 --run-seconds 退出" << std::endl;
            return 0;
        }

        std::cout << "[runtime-service] 运行中，按 Ctrl+C 停止" << std::endl;
        std::unique_lock<std::mutex> lock(g_shutdown_mutex);
        g_shutdown_cv.wait(lock, [] { return !g_keep_running.load(); });
        container.reset();
        std::cout << "[runtime-service] 已停止" << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[runtime-service] 错误: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[runtime-service] 未知错误" << std::endl;
        return 2;
    }
}
