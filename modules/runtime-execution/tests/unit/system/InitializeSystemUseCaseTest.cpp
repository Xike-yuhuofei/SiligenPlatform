#include "runtime_execution/application/usecases/system/InitializeSystemUseCase.h"

#include "gtest/gtest.h"

namespace {

using Siligen::Application::UseCases::System::InitializeSystemRequest;
using Siligen::Application::UseCases::System::InitializeSystemUseCase;
using Siligen::Application::UseCases::System::IHardLimitMonitor;
using Siligen::Application::UseCases::System::ISoftLimitMonitor;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
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

class FakeHardLimitMonitor final : public IHardLimitMonitor {
   public:
    Result<void> Start() noexcept override {
        ++start_calls;
        if (start_error.IsError()) {
            return Result<void>::Failure(start_error);
        }
        running = true;
        return Result<void>::Success();
    }

    Result<void> Stop() noexcept override {
        ++stop_calls;
        running = false;
        return Result<void>::Success();
    }

    bool IsRunning() const noexcept override {
        return running;
    }

    int start_calls = 0;
    int stop_calls = 0;
    bool running = false;
    Error start_error{};
};

class FakeSoftLimitMonitor final : public ISoftLimitMonitor {
   public:
    Result<void> Start() noexcept override {
        ++start_calls;
        if (start_error.IsError()) {
            return Result<void>::Failure(start_error);
        }
        running = true;
        return Result<void>::Success();
    }

    Result<void> Stop() noexcept override {
        ++stop_calls;
        running = false;
        return Result<void>::Success();
    }

    bool IsRunning() const noexcept override {
        return running;
    }

    int start_calls = 0;
    int stop_calls = 0;
    bool running = false;
    Error start_error{};
};

TEST(InitializeSystemUseCaseTest, StartsConnectionMonitoringAndHeartbeatFromSingleConnectionPort) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    InitializeSystemUseCase use_case(nullptr, connection_port, nullptr, nullptr, nullptr, nullptr, nullptr);

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

TEST(InitializeSystemUseCaseTest, StartsHardAndSoftLimitMonitoringWhenRequested) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto hard_limit_monitor = std::make_shared<FakeHardLimitMonitor>();
    auto soft_limit_monitor = std::make_shared<FakeSoftLimitMonitor>();
    InitializeSystemUseCase use_case(
        nullptr,
        connection_port,
        nullptr,
        nullptr,
        nullptr,
        hard_limit_monitor,
        soft_limit_monitor);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = true;
    request.start_heartbeat = false;
    request.start_status_monitoring = false;
    request.start_hard_limit_monitoring = true;
    request.require_hard_limit_monitoring = true;
    request.start_soft_limit_monitoring = true;
    request.require_soft_limit_monitoring = true;
    request.connection_config.local_ip = "192.168.0.10";
    request.connection_config.card_ip = "192.168.0.11";
    request.connection_config.local_port = 5000;
    request.connection_config.card_port = 5000;
    request.connection_config.timeout_ms = 1000;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& response = result.Value();
    EXPECT_TRUE(response.hardware_connected);
    EXPECT_TRUE(response.hard_limit_monitoring_started);
    EXPECT_TRUE(response.soft_limit_monitoring_started);
    EXPECT_EQ(hard_limit_monitor->start_calls, 1);
    EXPECT_EQ(soft_limit_monitor->start_calls, 1);
    EXPECT_EQ(hard_limit_monitor->stop_calls, 0);
    EXPECT_EQ(soft_limit_monitor->stop_calls, 0);
}

TEST(InitializeSystemUseCaseTest, RollsBackHardLimitMonitorIfSoftLimitMonitorFailsToStart) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto hard_limit_monitor = std::make_shared<FakeHardLimitMonitor>();
    auto soft_limit_monitor = std::make_shared<FakeSoftLimitMonitor>();
    soft_limit_monitor->start_error = Error(ErrorCode::THREAD_START_FAILED, "soft thread failed", "test");
    InitializeSystemUseCase use_case(
        nullptr,
        connection_port,
        nullptr,
        nullptr,
        nullptr,
        hard_limit_monitor,
        soft_limit_monitor);

    InitializeSystemRequest request;
    request.load_configuration = false;
    request.auto_connect_hardware = true;
    request.start_heartbeat = false;
    request.start_status_monitoring = false;
    request.start_hard_limit_monitoring = true;
    request.require_hard_limit_monitoring = true;
    request.start_soft_limit_monitoring = true;
    request.require_soft_limit_monitoring = true;
    request.connection_config.local_ip = "192.168.0.10";
    request.connection_config.card_ip = "192.168.0.11";
    request.connection_config.local_port = 5000;
    request.connection_config.card_port = 5000;
    request.connection_config.timeout_ms = 1000;

    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::THREAD_START_FAILED);
    EXPECT_EQ(hard_limit_monitor->start_calls, 1);
    EXPECT_EQ(hard_limit_monitor->stop_calls, 1);
    EXPECT_EQ(soft_limit_monitor->start_calls, 1);
}

}  // namespace
