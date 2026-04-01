#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using ILegacyMachineExecutionStateBackend = Siligen::Runtime::Host::System::ILegacyMachineExecutionStateBackend;
using LegacyMachineExecutionStateAdapter = Siligen::Runtime::Host::System::LegacyMachineExecutionStateAdapter;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using MachineExecutionSnapshot = Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;
using Result = Siligen::Shared::Types::Result<void>;
using SnapshotResult = Siligen::Shared::Types::Result<MachineExecutionSnapshot>;

class FakeMachineExecutionStateBackend final : public ILegacyMachineExecutionStateBackend {
   public:
    FakeMachineExecutionStateBackend() {
        snapshot_.phase = MachineExecutionPhase::Uninitialized;
        snapshot_.manual_motion_allowed = true;
        snapshot_.pending_task_count = 1;
        snapshot_.has_pending_tasks = true;
    }

    SnapshotResult ReadSnapshot() const override {
        return SnapshotResult::Success(snapshot_);
    }

    Result ClearPendingTasks() override {
        snapshot_.pending_task_count = 0;
        snapshot_.has_pending_tasks = false;
        return Result::Success();
    }

    Result TransitionToEmergencyStop() override {
        snapshot_.phase = MachineExecutionPhase::EmergencyStop;
        snapshot_.emergency_stopped = true;
        snapshot_.manual_motion_allowed = false;
        snapshot_.recent_error_summary = "machine_in_emergency_stop";
        return Result::Success();
    }

    Result RecoverToUninitialized() override {
        snapshot_.phase = MachineExecutionPhase::Uninitialized;
        snapshot_.emergency_stopped = false;
        snapshot_.manual_motion_allowed = true;
        snapshot_.recent_error_summary.clear();
        return Result::Success();
    }

   private:
    mutable MachineExecutionSnapshot snapshot_{};
};

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
    auto backend = std::make_shared<FakeMachineExecutionStateBackend>();
    LegacyMachineExecutionStateAdapter adapter(backend);

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

TEST(LegacyMachineExecutionStateAdapterTest, CanClearPendingTasksThroughBackendSeam) {
    auto backend = std::make_shared<FakeMachineExecutionStateBackend>();
    LegacyMachineExecutionStateAdapter adapter(backend);

    ASSERT_TRUE(adapter.ClearPendingTasks().IsSuccess());
    auto snapshot = adapter.ReadSnapshot();
    ASSERT_TRUE(snapshot.IsSuccess()) << snapshot.GetError().GetMessage();
    EXPECT_EQ(snapshot.Value().pending_task_count, 0);
    EXPECT_FALSE(snapshot.Value().has_pending_tasks);
}

}  // namespace
