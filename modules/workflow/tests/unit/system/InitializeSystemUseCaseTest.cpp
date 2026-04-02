#include "application/usecases/system/InitializeSystemUseCase.h"

#include "gtest/gtest.h"
#include "runtime_execution/contracts/motion/IHomeAxesExecutionPort.h"

#include <optional>

namespace {

using Siligen::Application::UseCases::System::InitializeSystemRequest;
using Siligen::Application::UseCases::System::InitializeSystemUseCase;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionRequest;
using Siligen::RuntimeExecution::Contracts::Motion::HomeAxesExecutionResponse;
using Siligen::RuntimeExecution::Contracts::Motion::IHomeAxesExecutionPort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

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

class FakeHomeAxesExecutionPort final : public IHomeAxesExecutionPort {
   public:
    Result<HomeAxesExecutionResponse> Execute(const HomeAxesExecutionRequest& request) override {
        ++execute_calls;
        last_request = request;
        if (execute_error.has_value()) {
            return Result<HomeAxesExecutionResponse>::Failure(execute_error.value());
        }
        return Result<HomeAxesExecutionResponse>::Success(response);
    }

    int execute_calls = 0;
    HomeAxesExecutionRequest last_request;
    HomeAxesExecutionResponse response;
    std::optional<Error> execute_error;
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

TEST(InitializeSystemUseCaseTest, AutoHomeUsesRuntimeContractPort) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto home_axes_port = std::make_shared<FakeHomeAxesExecutionPort>();
    home_axes_port->response.successfully_homed_axes = {LogicalAxisId::X, LogicalAxisId::Y};
    home_axes_port->response.all_completed = true;
    home_axes_port->response.status_message = "Homing completed";

    InitializeSystemUseCase use_case(nullptr, connection_port, home_axes_port, nullptr, nullptr, nullptr);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = false;
    request.start_heartbeat = false;
    request.auto_home_axes = true;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(result.Value().axes_homed);
    EXPECT_EQ(home_axes_port->execute_calls, 1);
    EXPECT_TRUE(home_axes_port->last_request.home_all_axes);
    EXPECT_TRUE(home_axes_port->last_request.wait_for_completion);
}

TEST(InitializeSystemUseCaseTest, AutoHomeFailsWhenRuntimePortReportsAxisFailures) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto home_axes_port = std::make_shared<FakeHomeAxesExecutionPort>();
    home_axes_port->response.failed_axes = {LogicalAxisId::X};
    home_axes_port->response.error_messages = {"X: Homing failed"};
    home_axes_port->response.all_completed = false;
    home_axes_port->response.status_message = "Homing completed with errors";

    InitializeSystemUseCase use_case(nullptr, connection_port, home_axes_port, nullptr, nullptr, nullptr);

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
