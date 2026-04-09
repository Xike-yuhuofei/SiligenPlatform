#include "runtime/system/MachineExecutionStateBackend.h"
#include "support/RuntimeExecutionHostTestSupport.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using MachineExecutionStateBackend = Siligen::Runtime::Service::System::MachineExecutionStateBackend;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;

TEST(RuntimeExecutionStateMachineRegressionTest, MapsReadyRunningPausedAndFaultLifecycleFromRuntimeOwnedStore) {
    auto store = Siligen::Runtime::Host::Tests::CreateMachineExecutionStateStore(MachineExecutionPhase::Ready, 1);
    ASSERT_NE(store, nullptr);

    MachineExecutionStateBackend port(store);

    auto ready_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(ready_snapshot.IsSuccess()) << ready_snapshot.GetError().GetMessage();
    EXPECT_EQ(ready_snapshot.Value().phase, MachineExecutionPhase::Ready);
    EXPECT_EQ(ready_snapshot.Value().pending_task_count, 1);
    EXPECT_TRUE(ready_snapshot.Value().has_pending_tasks);
    EXPECT_TRUE(ready_snapshot.Value().manual_motion_allowed);

    ASSERT_TRUE(store->SetPendingTaskCount(0).IsSuccess());
    ASSERT_TRUE(store->SetPhase(MachineExecutionPhase::Running).IsSuccess());
    auto running_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(running_snapshot.IsSuccess()) << running_snapshot.GetError().GetMessage();
    EXPECT_EQ(running_snapshot.Value().phase, MachineExecutionPhase::Running);
    EXPECT_EQ(running_snapshot.Value().pending_task_count, 0);
    EXPECT_FALSE(running_snapshot.Value().has_pending_tasks);

    ASSERT_TRUE(store->SetPhase(MachineExecutionPhase::Paused).IsSuccess());
    auto paused_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(paused_snapshot.IsSuccess()) << paused_snapshot.GetError().GetMessage();
    EXPECT_EQ(paused_snapshot.Value().phase, MachineExecutionPhase::Paused);
    EXPECT_TRUE(paused_snapshot.Value().manual_motion_allowed);

    ASSERT_TRUE(store->SetPhase(MachineExecutionPhase::Fault).IsSuccess());
    auto fault_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(fault_snapshot.IsSuccess()) << fault_snapshot.GetError().GetMessage();
    EXPECT_EQ(fault_snapshot.Value().phase, MachineExecutionPhase::Fault);
    EXPECT_FALSE(fault_snapshot.Value().manual_motion_allowed);
    EXPECT_EQ(fault_snapshot.Value().recent_error_summary, "machine_in_error_state");
}

TEST(RuntimeExecutionStateMachineRegressionTest, ClearsPendingTasksAndRecoversFromEmergencyStopThroughPort) {
    auto store = Siligen::Runtime::Host::Tests::CreateMachineExecutionStateStore(MachineExecutionPhase::Ready, 1);
    ASSERT_NE(store, nullptr);

    MachineExecutionStateBackend port(store);

    ASSERT_TRUE(port.ClearPendingTasks().IsSuccess());
    auto cleared_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(cleared_snapshot.IsSuccess()) << cleared_snapshot.GetError().GetMessage();
    EXPECT_EQ(cleared_snapshot.Value().pending_task_count, 0);
    EXPECT_FALSE(cleared_snapshot.Value().has_pending_tasks);

    ASSERT_TRUE(port.TransitionToEmergencyStop().IsSuccess());
    auto estop_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(estop_snapshot.IsSuccess()) << estop_snapshot.GetError().GetMessage();
    EXPECT_EQ(estop_snapshot.Value().phase, MachineExecutionPhase::EmergencyStop);
    EXPECT_TRUE(estop_snapshot.Value().emergency_stopped);
    EXPECT_FALSE(estop_snapshot.Value().manual_motion_allowed);
    EXPECT_EQ(estop_snapshot.Value().recent_error_summary, "machine_in_emergency_stop");

    ASSERT_TRUE(port.RecoverToUninitialized().IsSuccess());
    auto recovered_snapshot = port.ReadSnapshot();
    ASSERT_TRUE(recovered_snapshot.IsSuccess()) << recovered_snapshot.GetError().GetMessage();
    EXPECT_EQ(recovered_snapshot.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(recovered_snapshot.Value().emergency_stopped);
    EXPECT_TRUE(recovered_snapshot.Value().manual_motion_allowed);
}

}  // namespace
