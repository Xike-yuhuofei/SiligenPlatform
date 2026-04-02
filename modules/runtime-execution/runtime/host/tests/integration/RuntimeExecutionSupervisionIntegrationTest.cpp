#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "support/RuntimeExecutionHostTestSupport.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

namespace {

using ConnectionOnlyRuntimeSupervisionBackend = Siligen::Runtime::Host::Tests::ConnectionOnlyRuntimeSupervisionBackend;
using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;
using RuntimeSupervisionPortAdapter = Siligen::Runtime::Host::Supervision::RuntimeSupervisionPortAdapter;

TEST(RuntimeExecutionSupervisionIntegrationTest, ReportsDegradedSnapshotWhenHeartbeatFailsBeforeDisconnectSettles) {
    auto rig = Siligen::Runtime::Host::Tests::CreateMotionRuntimeTestRig();
    ASSERT_NE(rig.connection_adapter, nullptr);
    ASSERT_TRUE(rig.connection_adapter->Connect(Siligen::Runtime::Host::Tests::MakeMockDeviceConnection()).IsSuccess());

    auto supervision_backend = std::make_shared<ConnectionOnlyRuntimeSupervisionBackend>(rig.connection_adapter);
    RuntimeSupervisionPortAdapter supervision_port(supervision_backend);

    ASSERT_TRUE(rig.connection_adapter->StartHeartbeat(Siligen::Runtime::Host::Tests::MakeFastHeartbeatConfig())
                    .IsSuccess());

    rig.wrapper->ResetConnectionSimulation();
    rig.mock_card->SimulateDisconnect();

    ASSERT_TRUE(Siligen::Runtime::Host::Tests::WaitUntil(
        [&rig]() { return rig.connection_adapter->ReadHeartbeat().is_degraded; },
        std::chrono::milliseconds(1500)));

    const auto snapshot_result = supervision_port.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_FALSE(snapshot.connected);
    EXPECT_EQ(snapshot.connection_state, "degraded");
    EXPECT_EQ(snapshot.supervision.current_state, "Degraded");
    EXPECT_EQ(snapshot.supervision.state_reason, "heartbeat_degraded");
    EXPECT_EQ(snapshot.supervision.failure_code, "heartbeat_degraded");

    rig.connection_adapter->StopHeartbeat();
}

TEST(RuntimeExecutionSupervisionIntegrationTest, FallsBackToDisconnectedSnapshotAfterPassiveDisconnectIsObserved) {
    auto rig = Siligen::Runtime::Host::Tests::CreateMotionRuntimeTestRig();
    ASSERT_NE(rig.connection_adapter, nullptr);
    ASSERT_TRUE(rig.connection_adapter->Connect(Siligen::Runtime::Host::Tests::MakeMockDeviceConnection()).IsSuccess());

    auto supervision_backend = std::make_shared<ConnectionOnlyRuntimeSupervisionBackend>(rig.connection_adapter);
    RuntimeSupervisionPortAdapter supervision_port(supervision_backend);

    ASSERT_TRUE(rig.connection_adapter->StartHeartbeat(Siligen::Runtime::Host::Tests::MakeFastHeartbeatConfig())
                    .IsSuccess());
    ASSERT_TRUE(rig.connection_adapter->StartStatusMonitoring(250).IsSuccess());

    rig.wrapper->ResetConnectionSimulation();
    rig.mock_card->SimulateDisconnect();

    ASSERT_TRUE(Siligen::Runtime::Host::Tests::WaitUntil(
        [&rig]() { return rig.connection_adapter->ReadHeartbeat().is_degraded; },
        std::chrono::milliseconds(1500)));
    ASSERT_TRUE(Siligen::Runtime::Host::Tests::WaitUntil(
        [&rig]() {
            const auto connection_result = rig.connection_adapter->ReadConnection();
            return connection_result.IsSuccess() &&
                   connection_result.Value().state == DeviceConnectionState::Disconnected;
        },
        std::chrono::milliseconds(2500)));

    const auto snapshot_result = supervision_port.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_FALSE(snapshot.connected);
    EXPECT_EQ(snapshot.connection_state, "disconnected");
    EXPECT_EQ(snapshot.supervision.current_state, "Disconnected");
    EXPECT_EQ(snapshot.supervision.state_reason, "hardware_disconnected");
    EXPECT_TRUE(snapshot.supervision.failure_code.empty());

    rig.connection_adapter->StopStatusMonitoring();
    rig.connection_adapter->StopHeartbeat();
}

}  // namespace
