#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include "domain/machine/aggregates/DispenserModel.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using DispenserModel = Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel;
using LegacyMachineExecutionStateAdapter = Siligen::Runtime::Host::System::LegacyMachineExecutionStateAdapter;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;

TEST(LegacyMachineExecutionStateAdapterTest, DefaultAdapterStartsFromUninitializedSnapshot) {
    LegacyMachineExecutionStateAdapter adapter;

    auto snapshot_result = adapter.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();

    const auto& snapshot = snapshot_result.Value();
    EXPECT_EQ(snapshot.phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(snapshot.emergency_stopped);
    EXPECT_TRUE(snapshot.manual_motion_allowed);
    EXPECT_FALSE(snapshot.has_pending_tasks);
    EXPECT_EQ(snapshot.pending_task_count, 0);
}

TEST(LegacyMachineExecutionStateAdapterTest, CanTransitionToEmergencyStopAndRecover) {
    auto model = std::make_shared<DispenserModel>();
    LegacyMachineExecutionStateAdapter adapter(model);

    ASSERT_TRUE(adapter.TransitionToEmergencyStop().IsSuccess());
    auto estop_snapshot = adapter.ReadSnapshot();
    ASSERT_TRUE(estop_snapshot.IsSuccess()) << estop_snapshot.GetError().GetMessage();
    EXPECT_EQ(estop_snapshot.Value().phase, MachineExecutionPhase::EmergencyStop);
    EXPECT_TRUE(estop_snapshot.Value().emergency_stopped);
    EXPECT_FALSE(estop_snapshot.Value().manual_motion_allowed);
    EXPECT_EQ(estop_snapshot.Value().recent_error_summary, "machine_in_emergency_stop");

    ASSERT_TRUE(adapter.RecoverToUninitialized().IsSuccess());
    auto recovered_snapshot = adapter.ReadSnapshot();
    ASSERT_TRUE(recovered_snapshot.IsSuccess()) << recovered_snapshot.GetError().GetMessage();
    EXPECT_EQ(recovered_snapshot.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(recovered_snapshot.Value().emergency_stopped);
    EXPECT_TRUE(recovered_snapshot.Value().manual_motion_allowed);
}

}  // namespace
