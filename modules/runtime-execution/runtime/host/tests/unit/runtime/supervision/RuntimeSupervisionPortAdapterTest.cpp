#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using IRuntimeSupervisionBackend = Siligen::Runtime::Host::Supervision::IRuntimeSupervisionBackend;
using RuntimeSupervisionInputs = Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs;
using RuntimeSupervisionPortAdapter = Siligen::Runtime::Host::Supervision::RuntimeSupervisionPortAdapter;
using Snapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using SnapshotResult = Siligen::Shared::Types::Result<Snapshot>;

class FakeRuntimeSupervisionBackend final : public IRuntimeSupervisionBackend {
   public:
    RuntimeSupervisionInputs next_inputs{};

    Siligen::Shared::Types::Result<RuntimeSupervisionInputs> ReadInputs() const override {
        return Siligen::Shared::Types::Result<RuntimeSupervisionInputs>::Success(next_inputs);
    }
};

TEST(RuntimeSupervisionPortAdapterTest, MissingBackendReturnsNotInitializedError) {
    RuntimeSupervisionPortAdapter adapter(nullptr);

    auto snapshot_result = adapter.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsError());
    EXPECT_EQ(snapshot_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(RuntimeSupervisionPortAdapterTest, MapsHeartbeatDegradedIntoSupervisionSnapshot) {
    auto backend = std::make_shared<FakeRuntimeSupervisionBackend>();
    backend->next_inputs.has_hardware_connection_port = true;
    backend->next_inputs.connection_info.state = Siligen::Device::Contracts::State::DeviceConnectionState::Connected;
    backend->next_inputs.heartbeat_status.is_degraded = true;
    backend->next_inputs.io.door_known = true;

    RuntimeSupervisionPortAdapter adapter(backend);
    auto snapshot_result = adapter.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_EQ(snapshot.connection_state, "degraded");
    EXPECT_EQ(snapshot.supervision.current_state, "Degraded");
    EXPECT_EQ(snapshot.supervision.state_reason, "heartbeat_degraded");
    EXPECT_EQ(snapshot.supervision.failure_code, "heartbeat_degraded");
}

TEST(RuntimeSupervisionPortAdapterTest, MapsPausedJobIntoPausedSupervisionState) {
    auto backend = std::make_shared<FakeRuntimeSupervisionBackend>();
    backend->next_inputs.has_hardware_connection_port = true;
    backend->next_inputs.connected = true;
    backend->next_inputs.connection_info.state = Siligen::Device::Contracts::State::DeviceConnectionState::Connected;
    backend->next_inputs.io.door_known = true;
    backend->next_inputs.active_job_id = "job-1";
    backend->next_inputs.active_job_status_available = true;
    backend->next_inputs.active_job_state = "paused";

    RuntimeSupervisionPortAdapter adapter(backend);
    auto snapshot_result = adapter.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_EQ(snapshot.active_job_id, "job-1");
    EXPECT_EQ(snapshot.active_job_state, "paused");
    EXPECT_EQ(snapshot.supervision.current_state, "Paused");
    EXPECT_EQ(snapshot.supervision.state_reason, "job_paused");
}

TEST(RuntimeSupervisionPortAdapterTest, DoorInterlockOverridesPausedStateAndBuildsEffectiveInterlocks) {
    auto backend = std::make_shared<FakeRuntimeSupervisionBackend>();
    backend->next_inputs.has_hardware_connection_port = true;
    backend->next_inputs.connected = true;
    backend->next_inputs.connection_info.state = Siligen::Device::Contracts::State::DeviceConnectionState::Connected;
    backend->next_inputs.io.door_known = true;
    backend->next_inputs.io.door = true;
    backend->next_inputs.io.estop = false;
    backend->next_inputs.active_job_id = "job-2";
    backend->next_inputs.active_job_status_available = true;
    backend->next_inputs.active_job_state = "paused";
    backend->next_inputs.home_boundary_x_active = true;

    RuntimeSupervisionPortAdapter adapter(backend);
    auto snapshot_result = adapter.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_EQ(snapshot.supervision.current_state, "Fault");
    EXPECT_EQ(snapshot.supervision.state_reason, "interlock_door_open");
    EXPECT_TRUE(snapshot.effective_interlocks.door_open_active);
    EXPECT_TRUE(snapshot.effective_interlocks.home_boundary_x_active);
    ASSERT_EQ(snapshot.effective_interlocks.positive_escape_only_axes.size(), 1U);
    EXPECT_EQ(snapshot.effective_interlocks.positive_escape_only_axes.front(), "X");
}

TEST(RuntimeSupervisionPortAdapterTest, DisconnectedUnknownEstopRemainsUnknownInSnapshot) {
    auto backend = std::make_shared<FakeRuntimeSupervisionBackend>();
    backend->next_inputs.has_hardware_connection_port = true;
    backend->next_inputs.connected = false;
    backend->next_inputs.connection_info.state = Siligen::Device::Contracts::State::DeviceConnectionState::Disconnected;
    backend->next_inputs.io.estop = false;
    backend->next_inputs.io.estop_known = false;
    backend->next_inputs.io.door = false;
    backend->next_inputs.io.door_known = false;

    RuntimeSupervisionPortAdapter adapter(backend);
    auto snapshot_result = adapter.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    const auto& snapshot = snapshot_result.Value();
    EXPECT_EQ(snapshot.connection_state, "disconnected");
    EXPECT_FALSE(snapshot.io.estop_known);
    EXPECT_FALSE(snapshot.effective_interlocks.estop_known);
    EXPECT_EQ(snapshot.effective_interlocks.sources.at("estop"), "unknown");
    EXPECT_EQ(snapshot.supervision.current_state, "Disconnected");
    EXPECT_EQ(snapshot.supervision.state_reason, "hardware_disconnected");
}

}  // namespace
