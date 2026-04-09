#include "runtime/system/MachineExecutionStateBackend.h"
#include "runtime/system/MachineExecutionStateStore.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using MachineExecutionStateBackend = Siligen::Runtime::Service::System::MachineExecutionStateBackend;
using MachineExecutionStateStore = Siligen::Runtime::Service::System::MachineExecutionStateStore;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;

TEST(MachineExecutionStateBackendTest, NullInjectedStoreReturnsNotInitializedError) {
    MachineExecutionStateBackend backend(nullptr);

    const auto snapshot_result = backend.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsError());
    EXPECT_EQ(snapshot_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(MachineExecutionStateBackendTest, DefaultBackendStartsFromUninitializedSnapshot) {
    MachineExecutionStateBackend backend;

    const auto snapshot_result = backend.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    EXPECT_EQ(snapshot_result.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(snapshot_result.Value().emergency_stopped);
    EXPECT_TRUE(snapshot_result.Value().manual_motion_allowed);
    EXPECT_EQ(snapshot_result.Value().pending_task_count, 0);
    EXPECT_FALSE(snapshot_result.Value().has_pending_tasks);
}

TEST(MachineExecutionStateBackendTest, InjectedStoreMapsPendingTasksAndEmergencyLifecycle) {
    auto state_store = std::make_shared<MachineExecutionStateStore>();
    ASSERT_TRUE(state_store->SetPhase(MachineExecutionPhase::Ready).IsSuccess());
    ASSERT_TRUE(state_store->SetPendingTaskCount(1).IsSuccess());

    MachineExecutionStateBackend backend(state_store);

    auto ready_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(ready_snapshot.IsSuccess()) << ready_snapshot.GetError().GetMessage();
    EXPECT_EQ(ready_snapshot.Value().phase, MachineExecutionPhase::Ready);
    EXPECT_EQ(ready_snapshot.Value().pending_task_count, 1);
    EXPECT_TRUE(ready_snapshot.Value().has_pending_tasks);
    EXPECT_TRUE(ready_snapshot.Value().manual_motion_allowed);

    ASSERT_TRUE(backend.ClearPendingTasks().IsSuccess());
    auto cleared_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(cleared_snapshot.IsSuccess()) << cleared_snapshot.GetError().GetMessage();
    EXPECT_EQ(cleared_snapshot.Value().pending_task_count, 0);
    EXPECT_FALSE(cleared_snapshot.Value().has_pending_tasks);

    ASSERT_TRUE(backend.TransitionToEmergencyStop().IsSuccess());
    auto estop_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(estop_snapshot.IsSuccess()) << estop_snapshot.GetError().GetMessage();
    EXPECT_EQ(estop_snapshot.Value().phase, MachineExecutionPhase::EmergencyStop);
    EXPECT_TRUE(estop_snapshot.Value().emergency_stopped);
    EXPECT_FALSE(estop_snapshot.Value().manual_motion_allowed);
    EXPECT_EQ(estop_snapshot.Value().recent_error_summary, "machine_in_emergency_stop");

    ASSERT_TRUE(backend.RecoverToUninitialized().IsSuccess());
    auto recovered_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(recovered_snapshot.IsSuccess()) << recovered_snapshot.GetError().GetMessage();
    EXPECT_EQ(recovered_snapshot.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(recovered_snapshot.Value().emergency_stopped);
    EXPECT_TRUE(recovered_snapshot.Value().manual_motion_allowed);
}

}  // namespace
