#include "InitializeSystemUseCase.h"

#include "shared/types/Error.h"
#include "shared/types/Types.h"
#include "runtime_execution/contracts/motion/IHomeAxesExecutionPort.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

namespace Siligen::Application::UseCases::System {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

std::string JoinMessages(const std::vector<std::string>& messages, const std::string& delimiter) {
    std::ostringstream oss;
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i > 0) {
            oss << delimiter;
        }
        oss << messages[i];
    }
    return oss.str();
}

void PublishHardwareConnectionEvent(
    const std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort>& event_port,
    Siligen::Domain::System::Ports::EventType event_type,
    const Siligen::Device::Contracts::Commands::DeviceConnection& config,
    const Result<void>& connect_result) {
    if (!event_port) {
        return;
    }

    Siligen::Domain::System::Ports::HardwareConnectionEvent event;
    event.type = event_type;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.source = "InitializeSystemUseCase";
    event.hardware_id = "MultiCard";
    event.connection_info = config.card_ip + ":" + std::to_string(config.card_port);
    event.success = connect_result.IsSuccess();
    if (connect_result.IsError()) {
        event.error_code = static_cast<int32>(connect_result.GetError().GetCode());
        event.message = "Hardware connection failed: " + connect_result.GetError().GetMessage();
    } else {
        event.message = "Hardware connected successfully to " + event.connection_info;
    }

    event_port->PublishAsync(event);
}

Result<void> StartHeartbeatWithRetry(
    const std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort>& connection_port,
    const Siligen::Device::Contracts::State::HeartbeatSnapshot& config) {
    if (!connection_port) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Hardware connection port not initialized", "InitializeSystemUseCase"));
    }

    const int max_retries = 3;
    const std::chrono::milliseconds retry_delay(500);

    Result<void> heartbeat_result = Result<void>::Failure(
        Error(ErrorCode::COMMAND_FAILED, "Heartbeat not started", "InitializeSystemUseCase"));

    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        heartbeat_result = connection_port->StartHeartbeat(config);
        if (heartbeat_result.IsSuccess()) {
            break;
        }
        if (attempt < max_retries) {
            std::this_thread::sleep_for(retry_delay);
        }
    }

    return heartbeat_result;
}

}  // namespace

InitializeSystemUseCase::InitializeSystemUseCase(
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IHomeAxesExecutionPort> home_axes_execution_port,
    std::shared_ptr<Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort> diagnostics_port,
    std::shared_ptr<Siligen::Domain::System::Ports::IEventPublisherPort> event_port,
    std::shared_ptr<Siligen::Application::UseCases::System::IHardLimitMonitor> hard_limit_monitor)
    : config_port_(std::move(config_port)),
      connection_port_(std::move(connection_port)),
      home_axes_execution_port_(std::move(home_axes_execution_port)),
      diagnostics_port_(std::move(diagnostics_port)),
      event_port_(std::move(event_port)),
      hard_limit_monitor_(std::move(hard_limit_monitor)) {}

Result<InitializeSystemResponse> InitializeSystemUseCase::Execute(const InitializeSystemRequest& request) {
    if (!request.Validate()) {
        return Result<InitializeSystemResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid initialization request", "InitializeSystemUseCase"));
    }

    InitializeSystemResponse response;
    std::vector<std::string> warnings;

    // 1. 加载配置
    if (request.load_configuration) {
        if (!config_port_) {
            return Result<InitializeSystemResponse>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "Config port not initialized", "InitializeSystemUseCase"));
        }

        if (request.reload_configuration) {
            auto reload_result = config_port_->ReloadConfiguration();
            if (reload_result.IsError()) {
                return Result<InitializeSystemResponse>::Failure(reload_result.GetError());
            }
        } else {
            auto config_result = config_port_->LoadConfiguration();
            if (config_result.IsError()) {
                return Result<InitializeSystemResponse>::Failure(config_result.GetError());
            }
        }

        response.config_loaded = true;
    }

    if (request.validate_configuration) {
        if (!config_port_) {
            return Result<InitializeSystemResponse>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "Config port not initialized", "InitializeSystemUseCase"));
        }

        auto validation_result = config_port_->ValidateConfiguration();
        if (validation_result.IsError()) {
            return Result<InitializeSystemResponse>::Failure(validation_result.GetError());
        }

        response.config_validated = true;
        response.config_valid = validation_result.Value();
        if (!response.config_valid) {
            auto errors_result = config_port_->GetValidationErrors();
            if (errors_result.IsSuccess()) {
                response.config_validation_errors = errors_result.Value();
            }
            if (request.require_valid_configuration) {
                std::string message = "Configuration validation failed";
                if (!response.config_validation_errors.empty()) {
                    message += ": " + JoinMessages(response.config_validation_errors, "; ");
                }
                return Result<InitializeSystemResponse>::Failure(
                    Error(ErrorCode::INVALID_CONFIG_VALUE, message, "InitializeSystemUseCase"));
            }

            warnings.emplace_back("Configuration validation failed");
        }
    }

    // 2. 连接硬件（仅在显式请求时尝试）
    const bool connection_actions_requested = request.auto_connect_hardware || request.start_status_monitoring ||
                                              request.start_heartbeat;
    if (connection_actions_requested && !connection_port_) {
        return Result<InitializeSystemResponse>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Hardware connection port not initialized", "InitializeSystemUseCase"));
    }

    if (request.auto_connect_hardware) {
        auto already_connected = connection_port_->IsConnected();
        if (already_connected && request.force_reconnect) {
            auto disconnect_result = connection_port_->Disconnect();
            if (disconnect_result.IsError()) {
                return Result<InitializeSystemResponse>::Failure(disconnect_result.GetError());
            }
            already_connected = false;
        }

        if (!already_connected) {
            auto connect_result = connection_port_->Connect(request.connection_config);
            if (connect_result.IsError()) {
                PublishHardwareConnectionEvent(event_port_,
                                               Siligen::Domain::System::Ports::EventType::HARDWARE_CONNECTION_FAILED,
                                               request.connection_config,
                                               connect_result);
                return Result<InitializeSystemResponse>::Failure(connect_result.GetError());
            }

            PublishHardwareConnectionEvent(event_port_,
                                           Siligen::Domain::System::Ports::EventType::HARDWARE_CONNECTED,
                                           request.connection_config,
                                           connect_result);
        }

        response.hardware_connected = connection_port_->IsConnected();
        auto connection_result = connection_port_->ReadConnection();
        if (connection_result.IsError()) {
            return Result<InitializeSystemResponse>::Failure(connection_result.GetError());
        }
        response.connection_info = connection_result.Value();
    }

    if (request.start_hard_limit_monitoring) {
        if (!hard_limit_monitor_) {
            if (request.require_hard_limit_monitoring) {
                return Result<InitializeSystemResponse>::Failure(
                    Error(ErrorCode::PORT_NOT_INITIALIZED,
                          "Hard limit monitor not initialized",
                          "InitializeSystemUseCase"));
            }
            warnings.emplace_back("Hard limit monitor not available");
        } else if (connection_port_ && connection_port_->IsConnected()) {
            auto start_result = hard_limit_monitor_->Start();
            if (start_result.IsError()) {
                if (request.require_hard_limit_monitoring) {
                    return Result<InitializeSystemResponse>::Failure(start_result.GetError());
                }
                warnings.emplace_back("Hard limit monitor failed to start");
            } else {
                response.hard_limit_monitoring_started = true;
            }
        }
    }

    if (request.start_status_monitoring) {
        auto monitoring_result = connection_port_->StartStatusMonitoring(request.status_monitor_interval_ms);
        if (monitoring_result.IsError()) {
            if (request.require_status_monitoring) {
                return Result<InitializeSystemResponse>::Failure(monitoring_result.GetError());
            }
            warnings.emplace_back("Status monitoring failed to start");
        } else {
            response.status_monitoring_started = true;
        }
    }

    if (request.start_heartbeat) {
        auto heartbeat_result = StartHeartbeatWithRetry(connection_port_, request.heartbeat_config);
        if (heartbeat_result.IsError()) {
            if (request.require_heartbeat) {
                return Result<InitializeSystemResponse>::Failure(heartbeat_result.GetError());
            }
            warnings.emplace_back("Heartbeat failed to start");
        } else {
            response.heartbeat_started = true;
        }
    }

    if (connection_port_) {
        auto connection_result = connection_port_->ReadConnection();
        if (connection_result.IsError()) {
            return Result<InitializeSystemResponse>::Failure(connection_result.GetError());
        }
        response.connection_info = connection_result.Value();
        response.heartbeat_status = connection_port_->ReadHeartbeat();
        response.hardware_connected = response.connection_info.IsConnected();
    }

    if (request.run_diagnostics) {
        if (!diagnostics_port_) {
            return Result<InitializeSystemResponse>::Failure(
                Error(ErrorCode::PORT_NOT_INITIALIZED, "Diagnostics port not initialized", "InitializeSystemUseCase"));
        }

        auto health_result = diagnostics_port_->IsSystemHealthy();
        if (health_result.IsError()) {
            return Result<InitializeSystemResponse>::Failure(health_result.GetError());
        }

        response.diagnostics_performed = true;
        response.diagnostics_ok = health_result.Value();
        if (!response.diagnostics_ok) {
            if (request.require_healthy) {
                return Result<InitializeSystemResponse>::Failure(
                    Error(ErrorCode::INVALID_STATE, "System health check failed", "InitializeSystemUseCase"));
            }
            warnings.emplace_back("System health check failed");
        }
    }

    // 3. 回零（可选）
    if (request.auto_home_axes) {
        if (!home_axes_execution_port_) {
            return Result<InitializeSystemResponse>::Failure(
                Error(
                    ErrorCode::PORT_NOT_INITIALIZED,
                    "Home axes execution port not initialized",
                    "InitializeSystemUseCase"));
        }

        Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionRequest home_request;
        home_request.home_all_axes = true;
        home_request.wait_for_completion = true;

        auto homing_result = home_axes_execution_port_->Execute(home_request);
        if (homing_result.IsError()) {
            return Result<InitializeSystemResponse>::Failure(homing_result.GetError());
        }

        const auto& homing_response = homing_result.Value();
        if (!homing_response.failed_axes.empty()) {
            std::string message = "Homing completed with errors";
            if (!homing_response.error_messages.empty()) {
                message += ": " + JoinMessages(homing_response.error_messages, "; ");
            }
            return Result<InitializeSystemResponse>::Failure(
                Error(ErrorCode::HARDWARE_ERROR, message, "InitializeSystemUseCase"));
        }

        response.axes_homed = true;
    }

    response.status_message = warnings.empty()
        ? "System initialization completed"
        : "System initialization completed with warnings: " + JoinMessages(warnings, "; ");

    return Result<InitializeSystemResponse>::Success(response);
}

}  // namespace Siligen::Application::UseCases::System








