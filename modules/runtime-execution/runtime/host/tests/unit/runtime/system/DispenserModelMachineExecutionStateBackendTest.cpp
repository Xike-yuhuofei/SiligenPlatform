#include "runtime/system/DispenserModelMachineExecutionStateBackend.h"

#include "runtime/system/DispenserModel.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using DispenserModel = Siligen::Runtime::Service::System::DispenserModel;
using DispensingTask = Siligen::Runtime::Service::System::DispensingTask;
using CMPTriggerPoint = Siligen::Shared::Types::CMPTriggerPoint;
using DispenserModelMachineExecutionStateBackend = Siligen::Runtime::Service::System::DispenserModelMachineExecutionStateBackend;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;

DispensingTask MakePendingTask() {
    DispensingTask task;
    task.task_id = "task-1";
    task.path = {
        Siligen::Shared::Types::Point2D(0.0f, 0.0f),
        Siligen::Shared::Types::Point2D(10.0f, 0.0f),
    };
    task.cmp_config.AddTriggerPoint(CMPTriggerPoint{});
    task.movement_speed = 12.0f;
    return task;
}

TEST(DispenserModelMachineExecutionStateBackendTest, NullInjectedModelReturnsNotInitializedError) {
    DispenserModelMachineExecutionStateBackend backend(nullptr);

    const auto snapshot_result = backend.ReadSnapshot();
    ASSERT_TRUE(snapshot_result.IsError());
    EXPECT_EQ(snapshot_result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(DispenserModelMachineExecutionStateBackendTest, DefaultBackendStartsFromUninitializedSnapshot) {
    DispenserModelMachineExecutionStateBackend backend;

    const auto snapshot_result = backend.ReadSnapshot();

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().GetMessage();
    EXPECT_EQ(snapshot_result.Value().phase, MachineExecutionPhase::Uninitialized);
    EXPECT_FALSE(snapshot_result.Value().emergency_stopped);
    EXPECT_TRUE(snapshot_result.Value().manual_motion_allowed);
    EXPECT_EQ(snapshot_result.Value().pending_task_count, 0);
    EXPECT_FALSE(snapshot_result.Value().has_pending_tasks);
}

TEST(DispenserModelMachineExecutionStateBackendTest, InjectedModelMapsPendingTasksAndEmergencyLifecycle) {
    auto dispenser_model = std::make_shared<DispenserModel>();
    ASSERT_TRUE(dispenser_model->SetState(Siligen::DispenserState::INITIALIZING).IsSuccess());
    ASSERT_TRUE(dispenser_model->SetState(Siligen::DispenserState::READY).IsSuccess());
    ASSERT_TRUE(dispenser_model->AddTask(MakePendingTask()).IsSuccess());

    DispenserModelMachineExecutionStateBackend backend(dispenser_model);

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
