// InitializeSystemUseCase.h
// 版本: 1.0.0
// 描述: 系统初始化用例 - 应用层业务流程编排
//
// 六边形架构 - 应用层用例
// 依赖方向: 依赖领域端口,协调系统初始化流程

#pragma once

#include "runtime_execution/application/usecases/system/IHardLimitMonitor.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime/contracts/system/IEventPublisherPort.h"
#include "trace_diagnostics/contracts/IDiagnosticsPort.h"
#include "siligen/device/contracts/commands/device_commands.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "siligen/device/contracts/state/device_state.h"
#include "shared/types/Result.h"

#include <memory>
#include <string>
#include <vector>

namespace Siligen::Application::UseCases::Motion::Homing {
class HomeAxesUseCase;
}

namespace Siligen::Application::UseCases::System {

/// @brief 系统初始化请求参数
struct InitializeSystemRequest {
    // 配置相关
    bool load_configuration = true;         // 是否加载配置
    bool reload_configuration = false;      // 是否强制重新加载
    bool validate_configuration = false;    // 是否执行配置验证
    bool require_valid_configuration = false;  // 配置无效时是否失败

    // 硬件连接相关
    bool auto_connect_hardware = true;      // 是否自动连接硬件
    bool force_reconnect = false;           // 是否强制断开后重连
    Siligen::Device::Contracts::Commands::DeviceConnection connection_config;
    bool start_status_monitoring = false;   // 是否启动连接状态监控
    uint32 status_monitor_interval_ms = 1000;
    bool require_status_monitoring = false; // 监控启动失败是否视为错误

    bool start_heartbeat = true;            // 是否启动心跳
    Siligen::Device::Contracts::State::HeartbeatSnapshot heartbeat_config;
    bool require_heartbeat = false;         // 心跳启动失败是否视为错误

    // 硬限位监控
    bool start_hard_limit_monitoring = false;    // 是否启动硬限位监控
    bool require_hard_limit_monitoring = false;  // 监控启动失败是否视为错误

    // 诊断与回零
    bool run_diagnostics = false;           // 是否执行健康检查
    bool require_healthy = false;           // 健康检查失败是否视为错误
    bool auto_home_axes = false;            // 是否自动回零

    bool Validate() const {
        if (reload_configuration && !load_configuration) {
            return false;
        }
        if (start_status_monitoring && status_monitor_interval_ms == 0) {
            return false;
        }
        if (start_heartbeat && !heartbeat_config.IsValidConfig()) {
            return false;
        }
        if (auto_connect_hardware && !connection_config.IsValid()) {
            return false;
        }
        return true;
    }
};

/// @brief 系统初始化响应结果
struct InitializeSystemResponse {
    bool config_loaded;        // 配置是否加载成功
    bool config_validated;     // 是否执行配置验证
    bool config_valid;         // 配置是否有效
    bool hardware_connected;   // 硬件是否连接成功
    bool status_monitoring_started;  // 连接状态监控是否启动
    bool heartbeat_started;    // 心跳是否启动
    bool hard_limit_monitoring_started;  // 硬限位监控是否启动
    bool axes_homed;           // 轴是否回零成功
    bool diagnostics_performed; // 是否执行诊断
    bool diagnostics_ok;       // 诊断是否通过
    std::string status_message;
    Siligen::Device::Contracts::State::DeviceConnectionSnapshot connection_info;
    Siligen::Device::Contracts::State::HeartbeatSnapshot heartbeat_status;
    std::vector<std::string> config_validation_errors;

    InitializeSystemResponse()
        : config_loaded(false),
          config_validated(false),
          config_valid(true),
          hardware_connected(false),
          status_monitoring_started(false),
          heartbeat_started(false),
          hard_limit_monitoring_started(false),
          axes_homed(false),
          diagnostics_performed(false),
          diagnostics_ok(false),
          status_message(""),
          connection_info(),
          heartbeat_status(),
          config_validation_errors() {}
};

/// @brief 系统初始化用例
/// @details 应用层用例,协调配置加载、硬件连接、系统启动流程
///
/// 业务流程:
/// 1. 加载配置文件
/// 2. 连接硬件设备
/// 3. (可选)执行回零操作
/// 4. 初始化系统状态
class InitializeSystemUseCase {
   public:
    /// @brief 构造函数
    /// @param config_port 配置管理端口
    /// @param connection_port 硬件连接端口
    /// @param home_axes_usecase 回零用例
    /// @param diagnostics_port 诊断端口
    /// @param event_port 事件发布端口(可选)
    explicit InitializeSystemUseCase(
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase> home_axes_usecase,
        std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port,
        std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_port = nullptr,
        std::shared_ptr<Siligen::Application::UseCases::System::IHardLimitMonitor> hard_limit_monitor = nullptr);

    ~InitializeSystemUseCase() = default;

    // 禁止拷贝和移动
    InitializeSystemUseCase(const InitializeSystemUseCase&) = delete;
    InitializeSystemUseCase& operator=(const InitializeSystemUseCase&) = delete;
    InitializeSystemUseCase(InitializeSystemUseCase&&) = delete;
    InitializeSystemUseCase& operator=(InitializeSystemUseCase&&) = delete;

    /// @brief 执行系统初始化用例
    /// @param request 初始化请求参数
    /// @return Result<InitializeSystemResponse> 执行结果
    Siligen::Shared::Types::Result<InitializeSystemResponse> Execute(const InitializeSystemRequest& request);

   private:
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
    std::shared_ptr<Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase> home_axes_usecase_;
    std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port_;
    std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_port_;
    std::shared_ptr<Siligen::Application::UseCases::System::IHardLimitMonitor> hard_limit_monitor_;
};

}  // namespace Siligen::Application::UseCases::System
