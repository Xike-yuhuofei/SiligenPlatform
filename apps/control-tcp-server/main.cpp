// TCP Server app 主入口
// 架构: 六边形架构 - TCP Server作为主动适配器(Driving Adapter)
// 职责: App 层 - 组装依赖并启动服务

#include <boost/asio.hpp>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>

#include "ContainerBootstrap.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "siligen/gateway/tcp/tcp_facade_builder.h"
#include "siligen/gateway/tcp/tcp_server_host.h"

using Siligen::Gateway::Tcp::BuildTcpFacadeBundle;
using Siligen::Gateway::Tcp::TcpServerHost;
using Siligen::Gateway::Tcp::TcpServerHostOptions;

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "TcpServerMain"

#ifndef SILIGEN_GIT_HASH
#define SILIGEN_GIT_HASH "unknown"
#endif

namespace {

boost::asio::io_context* g_io_context = nullptr;

void SignalHandler(int signal) {
    std::cout << "\n[TCP Server] 收到信号 " << signal << ", 正在关闭..." << std::endl;
    if (g_io_context) {
        g_io_context->stop();
    }
}

void PrintBanner() {
    std::cout << "========================================\n"
              << "  Siligen TCP Server\n"
              << "  HMI通信适配器\n"
              << "========================================\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    uint16_t port = 9527;
    std::string config_path = "config/machine/machine_config.ini";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "用法: siligen_tcp_server [选项]\n"
                      << "选项:\n"
                      << "  -p, --port <端口>     TCP监听端口 (默认: 9527)\n"
                      << "  -c, --config <路径>   配置文件路径 (默认: config/machine/machine_config.ini)\n"
                      << "  -h, --help            显示帮助信息\n";
            return 0;
        }
    }

    PrintBanner();

    try {
        std::cout << "[TCP Server] 初始化应用容器..." << std::endl;
        auto container = Siligen::Apps::Runtime::BuildContainer(
            config_path,
            Siligen::Application::Container::LogMode::File,
            "logs/tcp_server.log"
        );
        std::cout << "[TCP Server] 应用容器配置完成" << std::endl;

        const char* boot_marker = "###BOOT_MARK:TCP_SERVER_DIAG_20260203###";
        const char* build_date = __DATE__;
        const char* build_time = __TIME__;
        std::string exe_path = std::filesystem::absolute(argv[0]).string();
        SILIGEN_LOG_INFO_FMT_HELPER("%s exe=%s config=%s port=%u",
                                    boot_marker,
                                    exe_path.c_str(),
                                    config_path.c_str(),
                                    port);
        SILIGEN_LOG_INFO_FMT_HELPER("%s build_date=%s build_time=%s git=%s",
                                    boot_marker,
                                    build_date,
                                    build_time,
                                    SILIGEN_GIT_HASH);

        std::cout << "[TCP Server] 构建 TCP 应用门面..." << std::endl;
        auto facades = BuildTcpFacadeBundle(*container);
        auto configPort = container->ResolvePort<Siligen::Domain::Configuration::Ports::IConfigurationPort>();

        std::cout << "[TCP Server] 创建传输宿主..." << std::endl;
        boost::asio::io_context io_context;
        g_io_context = &io_context;

        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);

        TcpServerHost server_host(
            io_context,
            facades,
            configPort,
            TcpServerHostOptions{port, config_path}
        );
        server_host.Start();

        std::cout << "[TCP Server] 服务器启动成功，监听端口: " << port << std::endl;
        std::cout << "[TCP Server] 按 Ctrl+C 停止服务器" << std::endl;

        io_context.run();

        std::cout << "[TCP Server] 服务器已停止" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[TCP Server] 错误: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[TCP Server] 未知错误" << std::endl;
        return 2;
    }
}
