#include "application/usecases/system/InitializeSystemUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"

#include "gtest/gtest.h"

#include <optional>
#include <unordered_map>

namespace {

using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using Siligen::Application::UseCases::System::InitializeSystemRequest;
using Siligen::Application::UseCases::System::InitializeSystemUseCase;
using Siligen::Domain::Configuration::Ports::DispensingConfig;
using Siligen::Domain::Configuration::Ports::DxfPreprocessConfig;
using Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::MachineConfig;
using Siligen::Domain::Configuration::Ports::SystemConfig;
using Siligen::Domain::Configuration::Ports::ValveSupplyConfig;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Shared::Types::DiagnosticsConfig;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::HardwareConfiguration;
using Siligen::Shared::Types::HardwareMode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VelocityTraceConfig;
using Siligen::Shared::Types::ValveCoordinationConfig;
using Siligen::Shared::Types::DispenserValveConfig;

class FakeConfigurationPort final : public IConfigurationPort {
   public:
    explicit FakeConfigurationPort(int axis_count) {
        hardware_config_.num_axes = axis_count;
        homing_configs_.reserve(axis_count);
        for (int axis = 0; axis < axis_count; ++axis) {
            HomingConfig config;
            config.axis = axis;
            homing_configs_.push_back(config);
        }
    }

    Result<SystemConfig> LoadConfiguration() override { return NotImplemented<SystemConfig>("LoadConfiguration"); }
    Result<void> SaveConfiguration(const SystemConfig& /*config*/) override { return NotImplementedVoid("SaveConfiguration"); }
    Result<void> ReloadConfiguration() override { return NotImplementedVoid("ReloadConfiguration"); }
    Result<DispensingConfig> GetDispensingConfig() const override { return NotImplemented<DispensingConfig>("GetDispensingConfig"); }
    Result<void> SetDispensingConfig(const DispensingConfig& /*config*/) override { return NotImplementedVoid("SetDispensingConfig"); }
    Result<DxfPreprocessConfig> GetDxfPreprocessConfig() const override { return Result<DxfPreprocessConfig>::Success({}); }
    Result<DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override { return Result<DxfTrajectoryConfig>::Success({}); }
    Result<DiagnosticsConfig> GetDiagnosticsConfig() const override { return NotImplemented<DiagnosticsConfig>("GetDiagnosticsConfig"); }
    Result<MachineConfig> GetMachineConfig() const override { return NotImplemented<MachineConfig>("GetMachineConfig"); }
    Result<void> SetMachineConfig(const MachineConfig& /*config*/) override { return NotImplementedVoid("SetMachineConfig"); }

    Result<HomingConfig> GetHomingConfig(int axis) const override {
        if (axis < 0 || axis >= static_cast<int>(homing_configs_.size())) {
            return Result<HomingConfig>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Homing config not found", "FakeConfigurationPort"));
        }
        return Result<HomingConfig>::Success(homing_configs_[static_cast<size_t>(axis)]);
    }

    Result<void> SetHomingConfig(int axis, const HomingConfig& config) override {
        if (axis < 0 || axis >= static_cast<int>(homing_configs_.size())) {
            return Result<void>::Failure(
                Error(ErrorCode::INVALID_PARAMETER, "Homing config not found", "FakeConfigurationPort"));
        }
        homing_configs_[static_cast<size_t>(axis)] = config;
        homing_configs_[static_cast<size_t>(axis)].axis = axis;
        return Result<void>::Success();
    }

    Result<std::vector<HomingConfig>> GetAllHomingConfigs() const override {
        return Result<std::vector<HomingConfig>>::Success(homing_configs_);
    }

    Result<ValveSupplyConfig> GetValveSupplyConfig() const override { return NotImplemented<ValveSupplyConfig>("GetValveSupplyConfig"); }
    Result<DispenserValveConfig> GetDispenserValveConfig() const override {
        return NotImplemented<DispenserValveConfig>("GetDispenserValveConfig");
    }
    Result<ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return NotImplemented<ValveCoordinationConfig>("GetValveCoordinationConfig");
    }
    Result<VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return NotImplemented<VelocityTraceConfig>("GetVelocityTraceConfig");
    }
    Result<bool> ValidateConfiguration() const override { return Result<bool>::Success(true); }
    Result<std::vector<std::string>> GetValidationErrors() const override {
        return Result<std::vector<std::string>>::Success({});
    }
    Result<void> BackupConfiguration(const std::string& /*backup_path*/) override { return NotImplementedVoid("BackupConfiguration"); }
    Result<void> RestoreConfiguration(const std::string& /*backup_path*/) override {
        return NotImplementedVoid("RestoreConfiguration");
    }
    Result<HardwareMode> GetHardwareMode() const override { return Result<HardwareMode>::Success(HardwareMode::Mock); }
    Result<HardwareConfiguration> GetHardwareConfiguration() const override {
        return Result<HardwareConfiguration>::Success(hardware_config_);
    }

   private:
    template <typename T>
    Result<T> NotImplemented(const char* method) const {
        return Result<T>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented", "FakeConfigurationPort"));
    }

    Result<void> NotImplementedVoid(const char* method) const {
        return Result<void>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, std::string(method) + " not implemented", "FakeConfigurationPort"));
    }

    HardwareConfiguration hardware_config_;
    std::vector<HomingConfig> homing_configs_;
};

class FakeHardwareConnectionPort final : public DeviceConnectionPort {
   public:
    Result<void> Connect(const DeviceConnection& config) override {
        ++connect_calls;
        last_config = config;
        connected = true;
        info.state = DeviceConnectionState::Connected;
        return Result<void>::Success();
    }

    Result<void> Disconnect() override {
        connected = false;
        info.state = DeviceConnectionState::Disconnected;
        return Result<void>::Success();
    }

    Result<DeviceConnectionSnapshot> ReadConnection() const override {
        return Result<DeviceConnectionSnapshot>::Success(info);
    }

    bool IsConnected() const override {
        return connected;
    }

    Result<void> Reconnect() override {
        ++reconnect_calls;
        connected = true;
        info.state = DeviceConnectionState::Connected;
        return Result<void>::Success();
    }

    void SetConnectionStateCallback(std::function<void(const DeviceConnectionSnapshot&)> /*callback*/) override {}

    Result<void> StartStatusMonitoring(uint32 interval_ms = 1000) override {
        ++monitor_calls;
        last_monitor_interval_ms = interval_ms;
        return Result<void>::Success();
    }

    void StopStatusMonitoring() override {}

    std::string GetLastError() const override {
        return {};
    }

    void ClearError() override {}

    Result<void> StartHeartbeat(const HeartbeatSnapshot& config) override {
        ++heartbeat_calls;
        heartbeat_status.is_active = true;
        heartbeat_status.interval_ms = config.interval_ms;
        heartbeat_status.failure_threshold = config.failure_threshold;
        return Result<void>::Success();
    }

    void StopHeartbeat() override {
        heartbeat_status.is_active = false;
    }

    HeartbeatSnapshot ReadHeartbeat() const override {
        return heartbeat_status;
    }

    Result<bool> Ping() const override {
        return Result<bool>::Success(connected);
    }

    int connect_calls = 0;
    int reconnect_calls = 0;
    int monitor_calls = 0;
    int heartbeat_calls = 0;
    uint32 last_monitor_interval_ms = 0;
    bool connected = false;
    DeviceConnection last_config;
    DeviceConnectionSnapshot info;
    HeartbeatSnapshot heartbeat_status;
};

class FakeHomingPort final : public IHomingPort {
   public:
    Result<void> HomeAxis(LogicalAxisId axis) override {
        ++home_axis_calls;
        last_home_axis = axis;
        homing_status_[axis].axis = axis;
        homing_status_[axis].state = HomingState::HOMING;
        return Result<void>::Success();
    }

    Result<void> StopHoming(LogicalAxisId /*axis*/) override { return Result<void>::Success(); }

    Result<void> ResetHomingState(LogicalAxisId axis) override {
        homing_status_[axis].axis = axis;
        homing_status_[axis].state = HomingState::NOT_HOMED;
        return Result<void>::Success();
    }

    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        auto it = homing_status_.find(axis);
        if (it == homing_status_.end()) {
            HomingStatus status;
            status.axis = axis;
            status.state = HomingState::NOT_HOMED;
            return Result<HomingStatus>::Success(status);
        }
        return Result<HomingStatus>::Success(it->second);
    }

    Result<bool> IsAxisHomed(LogicalAxisId axis) const override {
        auto it = homing_status_.find(axis);
        return Result<bool>::Success(it != homing_status_.end() && it->second.state == HomingState::HOMED);
    }

    Result<bool> IsHomingInProgress(LogicalAxisId axis) const override {
        auto it = homing_status_.find(axis);
        return Result<bool>::Success(it != homing_status_.end() && it->second.state == HomingState::HOMING);
    }

    Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 timeout_ms = 80000) override {
        ++wait_calls;
        last_wait_axis = axis;
        last_wait_timeout_ms = timeout_ms;

        if (wait_error.has_value()) {
            homing_status_[axis].axis = axis;
            homing_status_[axis].state = HomingState::FAILED;
            return Result<void>::Failure(wait_error.value());
        }

        homing_status_[axis].axis = axis;
        homing_status_[axis].state = HomingState::HOMED;
        return Result<void>::Success();
    }

    int home_axis_calls = 0;
    int wait_calls = 0;
    int32 last_wait_timeout_ms = 0;
    std::optional<LogicalAxisId> last_home_axis;
    std::optional<LogicalAxisId> last_wait_axis;
    std::optional<Error> wait_error;

   private:
    std::unordered_map<LogicalAxisId, HomingStatus> homing_status_;
};

TEST(InitializeSystemUseCaseTest, StartsConnectionMonitoringAndHeartbeatFromSingleConnectionPort) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    InitializeSystemUseCase use_case(nullptr, connection_port, nullptr, nullptr, nullptr, nullptr);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = true;
    request.start_status_monitoring = true;
    request.status_monitor_interval_ms = 250;
    request.start_heartbeat = true;
    request.connection_config.local_ip = "192.168.0.10";
    request.connection_config.card_ip = "192.168.0.11";
    request.connection_config.local_port = 5000;
    request.connection_config.card_port = 5000;
    request.connection_config.timeout_ms = 1000;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& response = result.Value();
    EXPECT_TRUE(response.hardware_connected);
    EXPECT_TRUE(response.status_monitoring_started);
    EXPECT_TRUE(response.heartbeat_started);
    EXPECT_EQ(connection_port->connect_calls, 1);
    EXPECT_EQ(connection_port->monitor_calls, 1);
    EXPECT_EQ(connection_port->heartbeat_calls, 1);
    EXPECT_EQ(connection_port->last_monitor_interval_ms, 250u);
}

TEST(InitializeSystemUseCaseTest, AutoHomeUsesHomeAxesUseCase) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto home_axes_usecase = std::make_shared<HomeAxesUseCase>(homing_port, config_port, nullptr, nullptr);

    InitializeSystemUseCase use_case(nullptr, connection_port, home_axes_usecase, nullptr, nullptr, nullptr);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = false;
    request.start_heartbeat = false;
    request.auto_home_axes = true;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(result.Value().axes_homed);
    ASSERT_TRUE(homing_port->last_home_axis.has_value());
    ASSERT_TRUE(homing_port->last_wait_axis.has_value());
    EXPECT_EQ(homing_port->home_axis_calls, 1);
    EXPECT_EQ(homing_port->wait_calls, 1);
    EXPECT_EQ(homing_port->last_home_axis.value(), LogicalAxisId::X);
    EXPECT_EQ(homing_port->last_wait_axis.value(), LogicalAxisId::X);
    EXPECT_EQ(homing_port->last_wait_timeout_ms, 80000);
}

TEST(InitializeSystemUseCaseTest, AutoHomeFailsWhenHomeAxesUseCaseReportsAxisFailures) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto config_port = std::make_shared<FakeConfigurationPort>(1);
    auto homing_port = std::make_shared<FakeHomingPort>();
    homing_port->wait_error = Error(ErrorCode::COMMAND_FAILED, "Homing failed", "FakeHomingPort");
    auto home_axes_usecase = std::make_shared<HomeAxesUseCase>(homing_port, config_port, nullptr, nullptr);

    InitializeSystemUseCase use_case(nullptr, connection_port, home_axes_usecase, nullptr, nullptr, nullptr);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = false;
    request.start_heartbeat = false;
    request.auto_home_axes = true;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::HARDWARE_ERROR);
    EXPECT_NE(result.GetError().GetMessage().find("Homing completed with errors"), std::string::npos);
}

}  // namespace
