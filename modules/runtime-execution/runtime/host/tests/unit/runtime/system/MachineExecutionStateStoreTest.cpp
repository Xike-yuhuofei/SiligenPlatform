#include "runtime/system/MachineExecutionStateStore.h"

#include <gtest/gtest.h>

namespace {

using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using MachineExecutionStateStore = Siligen::Runtime::Service::System::MachineExecutionStateStore;
using Siligen::Shared::Types::ErrorCode;

TEST(MachineExecutionStateStoreTest, DefaultStoreStartsFromUninitializedSnapshot) {
    MachineExecutionStateStore store;

    const auto snapshot = store.ReadSnapshot();
    EXPECT_EQ(snapshot.phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(snapshot.emergency_stopped);
    EXPECT_TRUE(snapshot.manual_motion_allowed);
    EXPECT_FALSE(snapshot.has_pending_tasks);
    EXPECT_EQ(snapshot.pending_task_count, 0);
    EXPECT_TRUE(snapshot.recent_error_summary.empty());
}

TEST(MachineExecutionStateStoreTest, SetPhaseUpdatesDerivedFields) {
    MachineExecutionStateStore store;

    ASSERT_TRUE(store.SetPhase(MachineExecutionPhase::Fault).IsSuccess());
    auto fault_snapshot = store.ReadSnapshot();
    EXPECT_EQ(fault_snapshot.phase, MachineExecutionPhase::Fault);
    EXPECT_FALSE(fault_snapshot.manual_motion_allowed);
    EXPECT_EQ(fault_snapshot.recent_error_summary, "machine_in_error_state");

    ASSERT_TRUE(store.TransitionToEmergencyStop().IsSuccess());
    auto estop_snapshot = store.ReadSnapshot();
    EXPECT_EQ(estop_snapshot.phase, MachineExecutionPhase::EmergencyStop);
    EXPECT_TRUE(estop_snapshot.emergency_stopped);
    EXPECT_FALSE(estop_snapshot.manual_motion_allowed);
    EXPECT_EQ(estop_snapshot.recent_error_summary, "machine_in_emergency_stop");
}

TEST(MachineExecutionStateStoreTest, SetPendingTaskCountRejectsNegativeValues) {
    MachineExecutionStateStore store;

    auto invalid_result = store.SetPendingTaskCount(-1);
    ASSERT_TRUE(invalid_result.IsError());
    EXPECT_EQ(invalid_result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);

    ASSERT_TRUE(store.SetPendingTaskCount(2).IsSuccess());
    const auto snapshot = store.ReadSnapshot();
    EXPECT_TRUE(snapshot.has_pending_tasks);
    EXPECT_EQ(snapshot.pending_task_count, 2);
}

TEST(MachineExecutionStateStoreTest, RecoverToUninitializedRequiresEmergencyStop) {
    MachineExecutionStateStore store;

    auto invalid_result = store.RecoverToUninitialized();
    ASSERT_TRUE(invalid_result.IsError());
    EXPECT_EQ(invalid_result.GetError().GetCode(), ErrorCode::INVALID_STATE);

    ASSERT_TRUE(store.TransitionToEmergencyStop().IsSuccess());
    ASSERT_TRUE(store.RecoverToUninitialized().IsSuccess());

    const auto snapshot = store.ReadSnapshot();
    EXPECT_EQ(snapshot.phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(snapshot.emergency_stopped);
    EXPECT_TRUE(snapshot.manual_motion_allowed);
    EXPECT_TRUE(snapshot.recent_error_summary.empty());
}

}  // namespace
