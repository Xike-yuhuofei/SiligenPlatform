#include "support/RuntimeExecutionHostTestSupport.h"

#include <gtest/gtest.h>

#include <chrono>

namespace {

using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;

TEST(RuntimeExecutionRecoveryRegressionTest, ReconnectsAfterPassiveDisconnect) {
    auto rig = Siligen::Runtime::Host::Tests::CreateMotionRuntimeTestRig();
    ASSERT_NE(rig.connection_adapter, nullptr);
    ASSERT_TRUE(rig.connection_adapter->Connect(Siligen::Runtime::Host::Tests::MakeMockDeviceConnection()).IsSuccess());
    ASSERT_TRUE(rig.connection_adapter->StartStatusMonitoring(50).IsSuccess());

    rig.wrapper->ResetConnectionSimulation();
    rig.mock_card->SimulateDisconnect();

    ASSERT_TRUE(Siligen::Runtime::Host::Tests::WaitUntil(
        [&rig]() {
            const auto connection_result = rig.connection_adapter->ReadConnection();
            return connection_result.IsSuccess() &&
                   connection_result.Value().state == DeviceConnectionState::Disconnected;
        },
        std::chrono::milliseconds(2000)));

    rig.wrapper->ResetConnectionSimulation();
    ASSERT_TRUE(rig.connection_adapter->Reconnect().IsSuccess());

    const auto recovered_connection = rig.connection_adapter->ReadConnection();
    ASSERT_TRUE(recovered_connection.IsSuccess()) << recovered_connection.GetError().GetMessage();
    EXPECT_EQ(recovered_connection.Value().state, DeviceConnectionState::Connected);
    EXPECT_TRUE(recovered_connection.Value().error_message.empty());
}

}  // namespace
