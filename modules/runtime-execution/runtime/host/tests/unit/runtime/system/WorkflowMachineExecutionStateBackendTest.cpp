#include "runtime/system/WorkflowMachineExecutionStateBackend.h"

#include "domain/machine/aggregates/DispenserModel.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using DispenserModel = Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel;
using DispensingTask = Siligen::Domain::Machine::Aggregates::Legacy::DispensingTask;
using WorkflowMachineExecutionStateBackend = Siligen::Runtime::Host::System::WorkflowMachineExecutionStateBackend;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;

DispensingTask MakeValidTask() {
    DispensingTask task;
    task.task_id = "task-1";
    task.path.emplace_back(0.0, 0.0);
    task.path.emplace_back(10.0, 5.0);
    task.cmp_config.AddTriggerPoint({});
    task.movement_speed = 10.0f;
    return task;
}

TEST(WorkflowMachineExecutionStateBackendTest, DefaultModelReportsUninitializedSnapshot) {
    WorkflowMachineExecutionStateBackend backend;

    auto snapshot_result = backend.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    EXPECT_EQ(snapshot_result.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_TRUE(snapshot_result.Value().manual_motion_allowed);
    EXPECT_FALSE(snapshot_result.Value().has_pending_tasks);
    EXPECT_EQ(snapshot_result.Value().pending_task_count, 0);
}

TEST(WorkflowMachineExecutionStateBackendTest, CanClearPendingTasksOnInjectedModel) {
    auto model = std::make_shared<DispenserModel>();
    auto add_result = model->AddTask(MakeValidTask());
    ASSERT_TRUE(add_result.IsSuccess()) << add_result.GetError().GetMessage();

    WorkflowMachineExecutionStateBackend backend(model);

    auto snapshot_before = backend.ReadSnapshot();
    ASSERT_TRUE(snapshot_before.IsSuccess()) << snapshot_before.GetError().GetMessage();
    EXPECT_TRUE(snapshot_before.Value().has_pending_tasks);
    EXPECT_EQ(snapshot_before.Value().pending_task_count, 1);

    auto clear_result = backend.ClearPendingTasks();
    ASSERT_TRUE(clear_result.IsSuccess()) << clear_result.GetError().GetMessage();

    auto snapshot_after = backend.ReadSnapshot();
    ASSERT_TRUE(snapshot_after.IsSuccess()) << snapshot_after.GetError().GetMessage();
    EXPECT_FALSE(snapshot_after.Value().has_pending_tasks);
    EXPECT_EQ(snapshot_after.Value().pending_task_count, 0);
}

TEST(WorkflowMachineExecutionStateBackendTest, CanTransitionToEmergencyStopAndRecover) {
    WorkflowMachineExecutionStateBackend backend;

    auto estop_result = backend.TransitionToEmergencyStop();
    ASSERT_TRUE(estop_result.IsSuccess()) << estop_result.GetError().GetMessage();

    auto estop_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(estop_snapshot.IsSuccess()) << estop_snapshot.GetError().GetMessage();
    EXPECT_EQ(estop_snapshot.Value().phase, MachineExecutionPhase::EmergencyStop);
    EXPECT_TRUE(estop_snapshot.Value().emergency_stopped);
    EXPECT_FALSE(estop_snapshot.Value().manual_motion_allowed);
    EXPECT_EQ(estop_snapshot.Value().recent_error_summary, "machine_in_emergency_stop");

    auto recover_result = backend.RecoverToUninitialized();
    ASSERT_TRUE(recover_result.IsSuccess()) << recover_result.GetError().GetMessage();

    auto recovered_snapshot = backend.ReadSnapshot();
    ASSERT_TRUE(recovered_snapshot.IsSuccess()) << recovered_snapshot.GetError().GetMessage();
    EXPECT_EQ(recovered_snapshot.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(recovered_snapshot.Value().emergency_stopped);
    EXPECT_TRUE(recovered_snapshot.Value().manual_motion_allowed);
    EXPECT_TRUE(recovered_snapshot.Value().recent_error_summary.empty());
}

}  // namespace
