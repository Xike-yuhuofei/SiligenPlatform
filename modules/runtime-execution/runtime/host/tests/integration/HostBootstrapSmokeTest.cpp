#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "runtime/system/DispenserModelMachineExecutionStateBackend.h"
#include "support/RuntimeExecutionHostTestSupport.h"

#include <gtest/gtest.h>

#include <memory>

namespace {

using ConnectionOnlyRuntimeSupervisionBackend = Siligen::Runtime::Host::Tests::ConnectionOnlyRuntimeSupervisionBackend;
using DispenserModel = Siligen::Runtime::Host::Tests::DispenserModel;
using DispenserModelMachineExecutionStateBackend = Siligen::Runtime::Service::System::DispenserModelMachineExecutionStateBackend;
using MachineExecutionPhase = Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using RuntimeSupervisionPortAdapter = Siligen::Runtime::Host::Supervision::RuntimeSupervisionPortAdapter;

TEST(RuntimeExecutionIntegrationHostBootstrapSmokeTest, ComposesCanonicalHostCorePortsWithoutAppBootstrap) {
    auto model = std::make_shared<DispenserModel>();
    ASSERT_TRUE(model->SetState(Siligen::DispenserState::INITIALIZING).IsSuccess());
    ASSERT_TRUE(model->SetState(Siligen::DispenserState::READY).IsSuccess());
    ASSERT_TRUE(model->AddTask(Siligen::Runtime::Host::Tests::MakePendingTask()).IsSuccess());

    DispenserModelMachineExecutionStateBackend machine_port(model);

    auto rig = Siligen::Runtime::Host::Tests::CreateMotionRuntimeTestRig();
    ASSERT_NE(rig.connection_adapter, nullptr);

    auto supervision_backend = std::make_shared<ConnectionOnlyRuntimeSupervisionBackend>(rig.connection_adapter);
    RuntimeSupervisionPortAdapter supervision_port(supervision_backend);

    const auto machine_snapshot_result = machine_port.ReadSnapshot();
    ASSERT_TRUE(machine_snapshot_result.IsSuccess()) << machine_snapshot_result.GetError().GetMessage();
    EXPECT_EQ(machine_snapshot_result.Value().phase, MachineExecutionPhase::Ready);
    EXPECT_EQ(machine_snapshot_result.Value().pending_task_count, 1);
    EXPECT_TRUE(machine_snapshot_result.Value().has_pending_tasks);

    const auto supervision_snapshot_result = supervision_port.ReadSnapshot();
    ASSERT_TRUE(supervision_snapshot_result.IsSuccess()) << supervision_snapshot_result.GetError().GetMessage();
    EXPECT_FALSE(supervision_snapshot_result.Value().connected);
    EXPECT_EQ(supervision_snapshot_result.Value().connection_state, "disconnected");
    EXPECT_EQ(supervision_snapshot_result.Value().supervision.current_state, "Disconnected");
    EXPECT_EQ(supervision_snapshot_result.Value().supervision.state_reason, "hardware_disconnected");
}

}  // namespace
