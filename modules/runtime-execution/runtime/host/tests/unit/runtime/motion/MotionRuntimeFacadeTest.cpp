#include "siligen/device/adapters/motion/MotionRuntimeFacade.h"

#include "domain/motion/ports/IHomingPort.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCard.h"
#include "siligen/device/adapters/drivers/multicard/MockMultiCardWrapper.h"
#include "siligen/device/adapters/motion/HomingPortAdapter.h"
#include "shared/types/HardwareConfiguration.h"
#include "domain/configuration/ports/IConfigurationPort.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace {

using HomingPortAdapter = Siligen::Infrastructure::Adapters::Motion::HomingPortAdapter;
using MotionRuntimeFacade = Siligen::Infrastructure::Adapters::Motion::MotionRuntimeFacade;
using MultiCardMotionAdapter = Siligen::Infrastructure::Adapters::MultiCardMotionAdapter;
using MockMultiCard = Siligen::Infrastructure::Hardware::MockMultiCard;
using MockMultiCardWrapper = Siligen::Infrastructure::Hardware::MockMultiCardWrapper;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;
using HardwareConnectionState = Siligen::Domain::Machine::Ports::HardwareConnectionState;
using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using HomingStatus = Siligen::Domain::Motion::Ports::HomingStatus;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using int32 = Siligen::Shared::Types::int32;
using HeartbeatConfig = Siligen::Domain::Machine::Ports::HeartbeatConfig;

class NoOpHomingPort final : public Siligen::Domain::Motion::Ports::IHomingPort {
   public:
    ResultVoid HomeAxis(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid StopHoming(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ResetHomingState(LogicalAxisId) override { return ResultVoid::Success(); }

    Siligen::Shared::Types::Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        return Siligen::Shared::Types::Result<HomingStatus>::Success(status);
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> IsHomingInProgress(LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    ResultVoid WaitForHomingComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }
};

TEST(MotionRuntimeFacadeTest, ReportsConnectedDuringGracePeriodAfterLegacyConnectSucceeds) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<NoOpHomingPort>());

    mock_card->SetGetStsReturnValue(-1);

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    auto adapter_connected = motion_adapter_result.Value()->IsConnected();
    ASSERT_TRUE(adapter_connected.IsSuccess());
    EXPECT_FALSE(adapter_connected.Value());

    auto runtime_connected = runtime->IsConnected();
    ASSERT_TRUE(runtime_connected.IsSuccess()) << runtime_connected.GetError().GetMessage();
    EXPECT_TRUE(runtime_connected.Value());
    EXPECT_TRUE(runtime->IsRuntimeConnected());

    auto axis_status = runtime->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(axis_status.IsSuccess()) << axis_status.GetError().GetMessage();
    EXPECT_FALSE(axis_status.Value().has_error);
    EXPECT_EQ(axis_status.Value().error_code, 0);
    EXPECT_NE(axis_status.Value().state, MotionState::FAULT);

    const auto info = runtime->GetRuntimeConnectionInfo();
    EXPECT_EQ(info.state, HardwareConnectionState::Connected);
    EXPECT_TRUE(info.IsConnected());
    EXPECT_TRUE(info.error_message.empty());
}

TEST(MotionRuntimeFacadeTest, HomeAxisDoesNotRecheckRawGetStsWhenRuntimeIsInGracePeriod) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].rapid_velocity = 50.0f;
    homing_configs[0].locate_velocity = 10.0f;
    homing_configs[0].acceleration = 100.0f;
    homing_configs[0].search_distance = 520.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].settle_time_ms = 10;

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<HomingPortAdapter>(wrapper, hardware_config, homing_configs));

    mock_card->SetGetStsReturnValue(-1);

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();
    ASSERT_TRUE(runtime->IsRuntimeConnected());

    auto home_result = runtime->HomeAxis(LogicalAxisId::X);
    ASSERT_TRUE(home_result.IsSuccess()) << home_result.GetError().GetMessage();
}

TEST(MotionRuntimeFacadeTest, DefaultMockHomeCompletesAndExposesHomedStatus) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    HardwareConfiguration hardware_config;
    hardware_config.num_axes = 2;
    hardware_config.pulse_per_mm = 200.0f;

    std::array<HomingConfig, 4> homing_configs{};
    homing_configs[0].axis = 0;
    homing_configs[0].rapid_velocity = 50.0f;
    homing_configs[0].locate_velocity = 10.0f;
    homing_configs[0].acceleration = 100.0f;
    homing_configs[0].search_distance = 520.0f;
    homing_configs[0].timeout_ms = 1000;
    homing_configs[0].settle_time_ms = 10;

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<HomingPortAdapter>(wrapper, hardware_config, homing_configs));

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    auto home_result = runtime->HomeAxis(LogicalAxisId::X);
    ASSERT_TRUE(home_result.IsSuccess()) << home_result.GetError().GetMessage();

    auto wait_result = runtime->WaitForHomingComplete(LogicalAxisId::X, 1000);
    ASSERT_TRUE(wait_result.IsSuccess()) << wait_result.GetError().GetMessage();

    auto homing_status = runtime->GetHomingStatus(LogicalAxisId::X);
    ASSERT_TRUE(homing_status.IsSuccess()) << homing_status.GetError().GetMessage();
    EXPECT_EQ(homing_status.Value().state, Siligen::Domain::Motion::Ports::HomingState::HOMED);

    auto axis_status = runtime->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(axis_status.IsSuccess()) << axis_status.GetError().GetMessage();
    EXPECT_EQ(axis_status.Value().state, MotionState::HOMED);

    auto axis_homed = runtime->IsAxisHomed(LogicalAxisId::X);
    ASSERT_TRUE(axis_homed.IsSuccess()) << axis_homed.GetError().GetMessage();
    EXPECT_TRUE(axis_homed.Value());
}

TEST(MotionRuntimeFacadeTest, InvokesConnectionStateCallbackOnConnectAndDisconnect) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<NoOpHomingPort>());

    std::vector<HardwareConnectionState> states;
    runtime->SetRuntimeConnectionStateCallback(
        [&states](const auto& info) {
            states.push_back(info.state);
        });

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    auto disconnect_result = runtime->Disconnect();
    ASSERT_TRUE(disconnect_result.IsSuccess()) << disconnect_result.GetError().GetMessage();

    ASSERT_EQ(states.size(), 2u);
    EXPECT_EQ(states[0], HardwareConnectionState::Connected);
    EXPECT_EQ(states[1], HardwareConnectionState::Disconnected);
}

TEST(MotionRuntimeFacadeTest, PassiveDisconnectNotifiesCallbackWhenStatusMonitoringIsActive) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<NoOpHomingPort>());

    std::condition_variable states_changed;
    std::mutex states_mutex;
    std::vector<HardwareConnectionState> states;
    runtime->SetRuntimeConnectionStateCallback(
        [&states, &states_changed, &states_mutex](const auto& info) {
            {
                std::lock_guard<std::mutex> lock(states_mutex);
                states.push_back(info.state);
            }
            states_changed.notify_all();
        });

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    auto monitoring_result = runtime->StartRuntimeStatusMonitoring(50);
    ASSERT_TRUE(monitoring_result.IsSuccess()) << monitoring_result.GetError().GetMessage();

    mock_card->ResetGetStsCounter();
    mock_card->SimulateDisconnect();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1600);
    {
        std::unique_lock<std::mutex> lock(states_mutex);
        (void)states_changed.wait_until(lock, deadline, [&states] {
            return states.size() >= 2u;
        });
    }

    runtime->StopRuntimeStatusMonitoring();

    std::vector<HardwareConnectionState> observed_states;
    {
        std::lock_guard<std::mutex> lock(states_mutex);
        observed_states = states;
    }

    ASSERT_GE(observed_states.size(), 2u);
    EXPECT_EQ(observed_states[0], HardwareConnectionState::Connected);
    EXPECT_EQ(observed_states.back(), HardwareConnectionState::Disconnected);
    EXPECT_FALSE(runtime->IsRuntimeConnected());
}

TEST(MotionRuntimeFacadeTest, PassiveDisconnectStillTransitionsToDisconnectedAfterHeartbeatDegrades) {
    auto mock_card = std::make_shared<MockMultiCard>();
    auto wrapper = std::make_shared<MockMultiCardWrapper>(mock_card);
    auto motion_adapter_result = MultiCardMotionAdapter::Create(wrapper);
    ASSERT_TRUE(motion_adapter_result.IsSuccess());

    auto runtime = std::make_shared<MotionRuntimeFacade>(
        motion_adapter_result.Value(),
        std::make_shared<NoOpHomingPort>());

    std::condition_variable states_changed;
    std::mutex states_mutex;
    std::vector<HardwareConnectionState> states;
    runtime->SetRuntimeConnectionStateCallback(
        [&states, &states_changed, &states_mutex](const auto& info) {
            {
                std::lock_guard<std::mutex> lock(states_mutex);
                states.push_back(info.state);
            }
            states_changed.notify_all();
        });

    auto connect_result = runtime->Connect("192.168.10.20", "192.168.10.10", 5000);
    ASSERT_TRUE(connect_result.IsSuccess()) << connect_result.GetError().GetMessage();

    HeartbeatConfig heartbeat_config;
    heartbeat_config.interval_ms = 100;
    heartbeat_config.timeout_ms = 100;
    heartbeat_config.failure_threshold = 1;
    auto heartbeat_result = runtime->StartRuntimeHeartbeat(heartbeat_config);
    ASSERT_TRUE(heartbeat_result.IsSuccess()) << heartbeat_result.GetError().GetMessage();

    auto monitoring_result = runtime->StartRuntimeStatusMonitoring(250);
    ASSERT_TRUE(monitoring_result.IsSuccess()) << monitoring_result.GetError().GetMessage();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    wrapper->ResetConnectionSimulation();
    mock_card->SimulateDisconnect();

    const auto heartbeat_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
    bool degraded_seen = false;
    while (std::chrono::steady_clock::now() < heartbeat_deadline) {
        if (runtime->GetRuntimeHeartbeatStatus().is_degraded) {
            degraded_seen = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    ASSERT_TRUE(degraded_seen);

    const auto disconnect_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1800);
    {
        std::unique_lock<std::mutex> lock(states_mutex);
        (void)states_changed.wait_until(lock, disconnect_deadline, [&states] {
            return std::find(states.begin(), states.end(), HardwareConnectionState::Disconnected) != states.end();
        });
    }

    runtime->StopRuntimeStatusMonitoring();
    runtime->StopRuntimeHeartbeat();

    std::vector<HardwareConnectionState> observed_states;
    {
        std::lock_guard<std::mutex> lock(states_mutex);
        observed_states = states;
    }

    ASSERT_GE(observed_states.size(), 2u);
    EXPECT_EQ(observed_states[0], HardwareConnectionState::Connected);
    EXPECT_EQ(observed_states.back(), HardwareConnectionState::Disconnected);
    EXPECT_EQ(runtime->GetRuntimeConnectionInfo().state, HardwareConnectionState::Disconnected);
    EXPECT_FALSE(runtime->IsRuntimeConnected());
}

}  // namespace
