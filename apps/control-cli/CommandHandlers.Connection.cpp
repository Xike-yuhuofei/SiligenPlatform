#include "CommandHandlers.h"
#include "CommandHandlersInternal.h"

#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "application/usecases/motion/initialization/MotionInitializationUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "application/usecases/system/InitializeSystemUseCase.h"

namespace Siligen::Adapters::CLI {

using Siligen::Application::CommandLineConfig;
using Siligen::Application::UseCases::Dispensing::Valve::ValveQueryUseCase;
using Siligen::Application::UseCases::Motion::Initialization::MotionInitializationUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Application::UseCases::System::InitializeSystemRequest;
using Siligen::Application::UseCases::System::InitializeSystemUseCase;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

Result<void> CLICommandHandlers::EnsureConnected(const CommandLineConfig& config) {
    auto init_usecase = container_->Resolve<MotionInitializationUseCase>();
    if (init_usecase) {
        auto connected = init_usecase->IsControllerConnected();
        if (connected.IsSuccess() && connected.Value()) {
            return Result<void>::Success();
        }
    }
    return ConnectHardware(config, true);
}

Result<void> CLICommandHandlers::ConnectHardware(const CommandLineConfig& config, bool verbose_output) {
    auto usecase = container_->Resolve<InitializeSystemUseCase>();
    if (!usecase) {
        return Result<void>::Failure(
            Error(ErrorCode::SERVICE_NOT_REGISTERED, "无法解析系统初始化用例", "CLI"));
    }

    InitializeSystemRequest request;
    request.auto_connect_hardware = config.auto_connect;
    request.start_heartbeat = config.start_heartbeat;
    request.start_status_monitoring = config.start_status_monitoring;
    request.auto_home_axes = config.auto_home_axes;
    request.status_monitor_interval_ms = static_cast<uint32>(config.status_monitor_interval_ms);
    request.heartbeat_config.interval_ms = static_cast<uint32>(config.heartbeat_interval_ms);
    request.heartbeat_config.timeout_ms = static_cast<uint32>(config.heartbeat_timeout_ms);
    request.heartbeat_config.failure_threshold = static_cast<uint32>(config.heartbeat_failure_threshold);

    request.connection_config.card_ip = config.control_card_ip;
    request.connection_config.local_ip = config.local_ip;
    if (config.port > 0) {
        request.connection_config.card_port = static_cast<uint16>(config.port);
        request.connection_config.local_port = static_cast<uint16>(config.port);
    }
    if (config.connect_timeout_ms > 0) {
        request.connection_config.timeout_ms = config.connect_timeout_ms;
    }

    auto result = usecase->Execute(request);
    if (result.IsError()) {
        return Result<void>::Failure(result.GetError());
    }
    if (!result.Value().hardware_connected) {
        return Result<void>::Failure(
            Error(ErrorCode::CONNECTION_FAILED, "硬件未连接", "CLI"));
    }

    if (verbose_output) {
        std::cout << "硬件连接成功" << std::endl;
    }
    return Result<void>::Success();
}

void CLICommandHandlers::ShowConnectionResult(const Result<void>& result) const {
    if (result.IsSuccess()) {
        std::cout << ">> 硬件连接成功" << std::endl;
        return;
    }

    std::cout << ">> 警告: 硬件连接失败 - " << result.GetError().GetMessage() << std::endl;
    std::cout << ">> 部分功能可能不可用" << std::endl;
}

void CLICommandHandlers::PrintError(const Error& error) const {
    std::cout << "错误代码: " << static_cast<int>(error.GetCode()) << std::endl;
    std::cout << "错误消息: " << error.GetMessage() << std::endl;
    if (!error.GetModule().empty()) {
        std::cout << "错误模块: " << error.GetModule() << std::endl;
    }
}

int CLICommandHandlers::HandleConnect(const CommandLineConfig& config) {
    auto result = ConnectHardware(config, true);
    ShowConnectionResult(result);
    return result.IsSuccess() ? 0 : 1;
}

int CLICommandHandlers::HandleDisconnect(const CommandLineConfig& config) {
    (void)config;
    auto init_usecase = container_->Resolve<MotionInitializationUseCase>();
    if (!init_usecase) {
        std::cout << "无法解析运动初始化用例" << std::endl;
        return 1;
    }

    auto result = init_usecase->DisconnectFromController();
    if (result.IsError()) {
        PrintError(result.GetError());
        return 1;
    }

    std::cout << "已断开硬件连接" << std::endl;
    return 0;
}

int CLICommandHandlers::HandleStatus(const CommandLineConfig& config) {
    auto ensure = EnsureConnected(config);
    if (ensure.IsError()) {
        PrintError(ensure.GetError());
        return 1;
    }

    bool show_motion = config.show_motion;
    bool show_io = config.show_io;
    bool show_valves = config.show_valves;
    if (!show_motion && !show_io && !show_valves) {
        show_motion = true;
        show_io = true;
        show_valves = true;
    }

    auto monitoring_usecase = container_->Resolve<MotionMonitoringUseCase>();
    if (!monitoring_usecase) {
        std::cout << "无法解析运动监控用例" << std::endl;
        return 1;
    }

    if (show_motion) {
        auto position_result = monitoring_usecase->GetCurrentPosition();
        if (position_result.IsSuccess()) {
            const auto pos = position_result.Value();
            std::cout << "当前位置: X=" << pos.x << " Y=" << pos.y << std::endl;
        }

        auto status_result = monitoring_usecase->GetAllAxesMotionStatus();
        if (status_result.IsError()) {
            PrintError(status_result.GetError());
        } else {
            const auto& statuses = status_result.Value();
            for (size_t i = 0; i < statuses.size(); ++i) {
                const auto& status = statuses[i];
                std::cout << "轴 " << (i + 1) << ": "
                          << MotionStateToString(status.state)
                          << " 速度=" << status.velocity
                          << " 使能=" << (status.enabled ? "是" : "否");
                if (status.has_error) {
                    std::cout << " 错误码=" << status.error_code;
                }
                std::cout << std::endl;
            }
        }
    }

    if (show_io) {
        auto inputs_result = monitoring_usecase->ReadAllDigitalInputStatus();
        if (inputs_result.IsSuccess()) {
            std::cout << "数字输入:" << std::endl;
            for (const auto& input : inputs_result.Value()) {
                std::cout << "  DI" << input.channel << ": " << (input.signal_active ? "ON" : "OFF") << std::endl;
            }
        }

        auto outputs_result = monitoring_usecase->ReadAllDigitalOutputStatus();
        if (outputs_result.IsSuccess()) {
            std::cout << "数字输出:" << std::endl;
            for (const auto& output : outputs_result.Value()) {
                std::cout << "  DO" << output.channel << ": " << (output.signal_active ? "ON" : "OFF") << std::endl;
            }
        }
    }

    if (show_valves) {
        auto valve_query_usecase = container_->Resolve<ValveQueryUseCase>();
        if (!valve_query_usecase) {
            std::cout << "无法解析阀状态查询用例" << std::endl;
            return 1;
        }

        auto dispenser_result = valve_query_usecase->GetDispenserStatus();
        if (dispenser_result.IsSuccess()) {
            const auto& state = dispenser_result.Value();
            std::cout << "点胶阀状态: " << DispenserStatusToString(state.status)
                      << " 进度=" << state.progress << "%" << std::endl;
        }

        auto supply_result = valve_query_usecase->GetSupplyStatus();
        if (supply_result.IsSuccess()) {
            const auto& status = supply_result.Value();
            std::cout << "供胶阀状态: " << SupplyStatusToString(status.state) << std::endl;
        }
    }

    return 0;
}

}  // namespace Siligen::Adapters::CLI
