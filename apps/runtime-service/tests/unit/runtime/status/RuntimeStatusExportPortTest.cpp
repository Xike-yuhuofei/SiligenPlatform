#include "runtime/status/RuntimeStatusExportPort.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using DispenserValveState = Siligen::Domain::Dispensing::Ports::DispenserValveState;
using DispenserValveStatus = Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using Point2D = Siligen::Shared::Types::Point2D;
using RuntimeDispenserStatusReader = Siligen::Runtime::Service::Status::RuntimeDispenserStatusReader;
using RuntimeJobExecutionExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot;
using RuntimeJobExecutionStatusReader =
    Siligen::Runtime::Service::Status::RuntimeJobExecutionStatusReader;
using RuntimeMotionStatusReader = Siligen::Runtime::Service::Status::RuntimeMotionStatusReader;
using RuntimeStatusExportPort = Siligen::Runtime::Service::Status::RuntimeStatusExportPort;
using RuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeSupervisionSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using SupplyValveState = Siligen::Domain::Dispensing::Ports::SupplyValveState;
using SupplyValveStatusDetail = Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail;

class FakeRuntimeSupervisionPort final : public RuntimeSupervisionPort {
   public:
    RuntimeSupervisionSnapshot next_snapshot{};

    Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot> ReadSnapshot() const override {
        return Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot>::Success(next_snapshot);
    }
};

struct FakeMotionReaderState {
    std::vector<MotionStatus> next_statuses;
    Point2D next_position{0.0f, 0.0f};
    bool fail_all_axes = false;
    bool fail_position = false;
    mutable int all_status_reads = 0;
    mutable int current_position_reads = 0;
};

struct FakeDispenserReaderState {
    DispenserValveState dispenser_state{};
    SupplyValveStatusDetail supply_state{};
    bool fail_dispenser = false;
    bool fail_supply = false;
    mutable int dispenser_reads = 0;
    mutable int supply_reads = 0;
};

struct FakeJobExecutionReaderState {
    std::unordered_map<std::string, RuntimeJobExecutionExportSnapshot> snapshots_by_job_id;
    bool fail_reads = false;
    mutable int read_count = 0;
    mutable std::string last_requested_job_id;
};

Siligen::Shared::Types::Error BuildTestError(const std::string& message) {
    return Siligen::Shared::Types::Error(
        Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
        message,
        "status-test");
}

RuntimeMotionStatusReader BuildMotionStatusReader(const std::shared_ptr<FakeMotionReaderState>& state) {
    RuntimeMotionStatusReader reader;
    reader.read_all_axes_motion_status = [state]() {
        ++state->all_status_reads;
        if (state->fail_all_axes) {
            return Siligen::Shared::Types::Result<std::vector<MotionStatus>>::Failure(
                BuildTestError("axes failed"));
        }
        return Siligen::Shared::Types::Result<std::vector<MotionStatus>>::Success(state->next_statuses);
    };
    reader.read_current_position = [state]() {
        ++state->current_position_reads;
        if (state->fail_position) {
            return Siligen::Shared::Types::Result<Point2D>::Failure(BuildTestError("position failed"));
        }
        return Siligen::Shared::Types::Result<Point2D>::Success(state->next_position);
    };
    return reader;
}

RuntimeDispenserStatusReader BuildDispenserStatusReader(const std::shared_ptr<FakeDispenserReaderState>& state) {
    RuntimeDispenserStatusReader reader;
    reader.read_dispenser_status = [state]() {
        ++state->dispenser_reads;
        if (state->fail_dispenser) {
            return Siligen::Shared::Types::Result<DispenserValveState>::Failure(
                BuildTestError("dispenser failed"));
        }
        return Siligen::Shared::Types::Result<DispenserValveState>::Success(state->dispenser_state);
    };
    reader.read_supply_status = [state]() {
        ++state->supply_reads;
        if (state->fail_supply) {
            return Siligen::Shared::Types::Result<SupplyValveStatusDetail>::Failure(
                BuildTestError("supply failed"));
        }
        return Siligen::Shared::Types::Result<SupplyValveStatusDetail>::Success(state->supply_state);
    };
    return reader;
}

RuntimeJobExecutionStatusReader BuildJobExecutionStatusReader(
    const std::shared_ptr<FakeJobExecutionReaderState>& state) {
    RuntimeJobExecutionStatusReader reader;
    reader.read_job_status = [state](const std::string& job_id) {
        ++state->read_count;
        state->last_requested_job_id = job_id;
        if (state->fail_reads) {
            return Siligen::Shared::Types::Result<RuntimeJobExecutionExportSnapshot>::Failure(
                BuildTestError("job execution failed"));
        }
        const auto it = state->snapshots_by_job_id.find(job_id);
        if (it == state->snapshots_by_job_id.end()) {
            return Siligen::Shared::Types::Result<RuntimeJobExecutionExportSnapshot>::Failure(
                BuildTestError("job execution missing"));
        }
        return Siligen::Shared::Types::Result<RuntimeJobExecutionExportSnapshot>::Success(it->second);
    };
    return reader;
}

RuntimeJobExecutionExportSnapshot BuildJobExecutionSnapshot(
    const std::string& job_id,
    const std::string& state,
    std::uint32_t completed_count = 0,
    std::uint32_t overall_progress_percent = 0,
    bool dry_run = false) {
    RuntimeJobExecutionExportSnapshot snapshot;
    snapshot.job_id = job_id;
    snapshot.plan_id = "plan-" + job_id;
    snapshot.plan_fingerprint = "hash-" + job_id;
    snapshot.state = state;
    snapshot.target_count = 2;
    snapshot.completed_count = completed_count;
    snapshot.current_cycle = completed_count + (state == "running" ? 1U : 0U);
    snapshot.current_segment = overall_progress_percent;
    snapshot.total_segments = 100;
    snapshot.cycle_progress_percent = overall_progress_percent;
    snapshot.overall_progress_percent = overall_progress_percent;
    snapshot.elapsed_seconds = 3.5;
    snapshot.error_message = state == "failed" ? "job failed" : "";
    snapshot.dry_run = dry_run;
    return snapshot;
}

void SetKnownSafeGateSignals(RuntimeSupervisionSnapshot& snapshot) {
    snapshot.io.estop = false;
    snapshot.io.estop_known = true;
    snapshot.io.door = false;
    snapshot.io.door_known = true;
    snapshot.effective_interlocks.estop_active = false;
    snapshot.effective_interlocks.estop_known = true;
    snapshot.effective_interlocks.door_open_active = false;
    snapshot.effective_interlocks.door_open_known = true;
}

TEST(RuntimeStatusExportPortTest, MissingSupervisionPortReturnsNotInitializedError) {
    RuntimeStatusExportPort port(nullptr);

    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(RuntimeStatusExportPortTest, ConnectedSnapshotIncludesAuthorityAndOptionalEnrichment) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.interlock_latched = true;
    supervision_port->next_snapshot.active_job_id = "job-42";
    supervision_port->next_snapshot.active_job_state = "running";
    supervision_port->next_snapshot.supervision.current_state = "Running";
    supervision_port->next_snapshot.supervision.state_reason = "executing";
    SetKnownSafeGateSignals(supervision_port->next_snapshot);

    auto motion_reader_state = std::make_shared<FakeMotionReaderState>();
    MotionStatus x_status;
    x_status.position.x = 12.5f;
    x_status.velocity = 4.0f;
    x_status.enabled = true;
    x_status.homing_state = "homed";
    MotionStatus y_status;
    y_status.position.x = 8.0f;
    y_status.velocity = 1.5f;
    y_status.enabled = false;
    y_status.homing_state = "searching";
    motion_reader_state->next_statuses = {x_status, y_status};
    motion_reader_state->next_position = Point2D{12.5f, 8.0f};

    auto dispenser_reader_state = std::make_shared<FakeDispenserReaderState>();
    dispenser_reader_state->dispenser_state.status = DispenserValveStatus::Running;
    dispenser_reader_state->dispenser_state.completedCount = 8;
    dispenser_reader_state->dispenser_state.totalCount = 20;
    dispenser_reader_state->dispenser_state.progress = 40.0f;
    dispenser_reader_state->supply_state.state = SupplyValveState::Open;

    auto job_execution_reader_state = std::make_shared<FakeJobExecutionReaderState>();
    job_execution_reader_state->snapshots_by_job_id.emplace(
        "job-42",
        BuildJobExecutionSnapshot("job-42", "running", 1, 55));

    RuntimeStatusExportPort port(
        supervision_port,
        BuildMotionStatusReader(motion_reader_state),
        BuildDispenserStatusReader(dispenser_reader_state),
        BuildJobExecutionStatusReader(job_execution_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& snapshot = result.Value();
    EXPECT_TRUE(snapshot.connected);
    EXPECT_EQ(snapshot.connection_state, "connected");
    EXPECT_EQ(snapshot.device_mode, "production");
    EXPECT_EQ(snapshot.machine_state, "Running");
    EXPECT_EQ(snapshot.machine_state_reason, "executing");
    EXPECT_FALSE(snapshot.runtime_identity.executable_path.empty());
    EXPECT_FALSE(snapshot.runtime_identity.working_directory.empty());
    EXPECT_EQ(snapshot.runtime_identity.protocol_version, "siligen.application/1.0");
    EXPECT_EQ(
        snapshot.runtime_identity.preview_snapshot_contract,
        "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline");
    EXPECT_TRUE(snapshot.interlock_latched);
    EXPECT_EQ(snapshot.supervision.current_state, "Running");
    EXPECT_EQ(snapshot.supervision.state_reason, "executing");
    ASSERT_EQ(snapshot.axes.size(), 2U);
    EXPECT_TRUE(snapshot.axes.at("X").homed);
    EXPECT_EQ(snapshot.axes.at("X").homing_state, "homed");
    EXPECT_FALSE(snapshot.axes.at("Y").homed);
    EXPECT_EQ(snapshot.axes.at("Y").homing_state, "searching");
    EXPECT_TRUE(snapshot.has_position);
    EXPECT_DOUBLE_EQ(snapshot.position.x, 12.5);
    EXPECT_DOUBLE_EQ(snapshot.position.y, 8.0);
    EXPECT_TRUE(snapshot.dispenser.valve_open);
    EXPECT_TRUE(snapshot.dispenser.supply_open);
    EXPECT_EQ(snapshot.dispenser.completedCount, 8U);
    EXPECT_EQ(snapshot.dispenser.totalCount, 20U);
    EXPECT_DOUBLE_EQ(snapshot.dispenser.progress, 40.0);
    EXPECT_EQ(snapshot.job_execution.job_id, "job-42");
    EXPECT_EQ(snapshot.job_execution.state, "running");
    EXPECT_EQ(snapshot.job_execution.completed_count, 1U);
    EXPECT_EQ(snapshot.job_execution.overall_progress_percent, 55U);
    EXPECT_EQ(snapshot.safety_boundary.state, "blocked");
    EXPECT_FALSE(snapshot.safety_boundary.motion_permitted);
    EXPECT_FALSE(snapshot.safety_boundary.process_output_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.motion_commands_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_TRUE(snapshot.action_capabilities.active_job_present);
    EXPECT_FALSE(snapshot.action_capabilities.estop_reset_permitted);
    EXPECT_TRUE(snapshot.safety_boundary.interlock_latched);
    EXPECT_EQ(snapshot.safety_boundary.blocking_reasons, std::vector<std::string>({"interlock_latched"}));
    EXPECT_EQ(job_execution_reader_state->last_requested_job_id, "job-42");
}

TEST(RuntimeStatusExportPortTest, DisconnectedSnapshotSkipsMotionReadsAndReturnsIdleJobExecution) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = false;
    supervision_port->next_snapshot.connection_state = "degraded";

    auto motion_reader_state = std::make_shared<FakeMotionReaderState>();
    RuntimeStatusExportPort port(supervision_port, BuildMotionStatusReader(motion_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& snapshot = result.Value();
    EXPECT_FALSE(snapshot.connected);
    EXPECT_EQ(snapshot.connection_state, "degraded");
    EXPECT_EQ(snapshot.device_mode, "production");
    EXPECT_EQ(snapshot.machine_state, "Unknown");
    EXPECT_EQ(snapshot.machine_state_reason, "unknown");
    EXPECT_FALSE(snapshot.runtime_identity.executable_path.empty());
    EXPECT_FALSE(snapshot.runtime_identity.working_directory.empty());
    EXPECT_EQ(snapshot.runtime_identity.protocol_version, "siligen.application/1.0");
    EXPECT_EQ(
        snapshot.runtime_identity.preview_snapshot_contract,
        "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline");
    EXPECT_TRUE(snapshot.axes.empty());
    EXPECT_FALSE(snapshot.has_position);
    EXPECT_EQ(snapshot.job_execution.state, "idle");
    EXPECT_EQ(snapshot.safety_boundary.state, "unknown");
    EXPECT_FALSE(snapshot.safety_boundary.motion_permitted);
    EXPECT_FALSE(snapshot.safety_boundary.process_output_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.motion_commands_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_FALSE(snapshot.action_capabilities.active_job_present);
    EXPECT_FALSE(snapshot.action_capabilities.estop_reset_permitted);
    EXPECT_EQ(
        snapshot.safety_boundary.blocking_reasons,
        std::vector<std::string>({"estop_unknown", "door_unknown"}));
    EXPECT_EQ(motion_reader_state->all_status_reads, 0);
    EXPECT_EQ(motion_reader_state->current_position_reads, 0);
}

TEST(RuntimeStatusExportPortTest, MissingOptionalReadersStillReturnsAuthoritySnapshot) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Idle";
    supervision_port->next_snapshot.supervision.state_reason = "ready";
    SetKnownSafeGateSignals(supervision_port->next_snapshot);

    RuntimeStatusExportPort port(supervision_port);
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().device_mode, "production");
    EXPECT_EQ(result.Value().machine_state, "Idle");
    EXPECT_EQ(result.Value().machine_state_reason, "ready");
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_FALSE(result.Value().dispenser.valve_open);
    EXPECT_FALSE(result.Value().dispenser.supply_open);
    EXPECT_EQ(result.Value().dispenser.completedCount, 0U);
    EXPECT_EQ(result.Value().dispenser.totalCount, 0U);
    EXPECT_DOUBLE_EQ(result.Value().dispenser.progress, 0.0);
    EXPECT_EQ(result.Value().job_execution.state, "idle");
    EXPECT_EQ(result.Value().safety_boundary.state, "safe");
    EXPECT_TRUE(result.Value().safety_boundary.motion_permitted);
    EXPECT_TRUE(result.Value().safety_boundary.process_output_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.motion_commands_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.active_job_present);
    EXPECT_FALSE(result.Value().action_capabilities.estop_reset_permitted);
    EXPECT_TRUE(result.Value().safety_boundary.blocking_reasons.empty());
}

TEST(RuntimeStatusExportPortTest, OptionalReaderFailuresDoNotFailSnapshot) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.active_job_id = "job-failed";
    supervision_port->next_snapshot.active_job_state = "running";
    supervision_port->next_snapshot.supervision.current_state = "Running";
    supervision_port->next_snapshot.supervision.state_reason = "authoritative";

    auto motion_reader_state = std::make_shared<FakeMotionReaderState>();
    motion_reader_state->fail_all_axes = true;
    motion_reader_state->fail_position = true;

    auto dispenser_reader_state = std::make_shared<FakeDispenserReaderState>();
    dispenser_reader_state->fail_dispenser = true;
    dispenser_reader_state->fail_supply = true;

    auto job_execution_reader_state = std::make_shared<FakeJobExecutionReaderState>();
    job_execution_reader_state->fail_reads = true;

    RuntimeStatusExportPort port(
        supervision_port,
        BuildMotionStatusReader(motion_reader_state),
        BuildDispenserStatusReader(dispenser_reader_state),
        BuildJobExecutionStatusReader(job_execution_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().device_mode, "production");
    EXPECT_EQ(result.Value().machine_state, "Running");
    EXPECT_EQ(result.Value().machine_state_reason, "authoritative");
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_FALSE(result.Value().dispenser.valve_open);
    EXPECT_FALSE(result.Value().dispenser.supply_open);
    EXPECT_EQ(result.Value().dispenser.completedCount, 0U);
    EXPECT_EQ(result.Value().dispenser.totalCount, 0U);
    EXPECT_DOUBLE_EQ(result.Value().dispenser.progress, 0.0);
    EXPECT_EQ(result.Value().job_execution.job_id, "job-failed");
    EXPECT_EQ(result.Value().job_execution.state, "unknown");
    EXPECT_EQ(result.Value().job_execution.error_message, "job execution failed");
    EXPECT_FALSE(result.Value().action_capabilities.motion_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.active_job_present);
    EXPECT_FALSE(result.Value().action_capabilities.estop_reset_permitted);
    EXPECT_EQ(motion_reader_state->all_status_reads, 1);
    EXPECT_EQ(motion_reader_state->current_position_reads, 1);
    EXPECT_EQ(dispenser_reader_state->dispenser_reads, 1);
    EXPECT_EQ(dispenser_reader_state->supply_reads, 1);
    EXPECT_EQ(job_execution_reader_state->read_count, 1);
}

TEST(RuntimeStatusExportPortTest, ActiveDryRunJobExportsTestDeviceMode) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.active_job_id = "job-test";
    supervision_port->next_snapshot.active_job_state = "running";
    supervision_port->next_snapshot.supervision.current_state = "Running";
    supervision_port->next_snapshot.supervision.state_reason = "executing";
    SetKnownSafeGateSignals(supervision_port->next_snapshot);

    auto job_execution_reader_state = std::make_shared<FakeJobExecutionReaderState>();
    job_execution_reader_state->snapshots_by_job_id.emplace(
        "job-test",
        BuildJobExecutionSnapshot("job-test", "running", 0, 25, true));

    RuntimeStatusExportPort port(
        supervision_port,
        {},
        {},
        BuildJobExecutionStatusReader(job_execution_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().device_mode, "test");
    EXPECT_TRUE(result.Value().job_execution.dry_run);
    EXPECT_EQ(result.Value().safety_boundary.state, "safe");
    EXPECT_TRUE(result.Value().safety_boundary.motion_permitted);
    EXPECT_FALSE(result.Value().safety_boundary.process_output_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.motion_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.active_job_present);
    EXPECT_FALSE(result.Value().action_capabilities.estop_reset_permitted);
    EXPECT_TRUE(result.Value().safety_boundary.blocking_reasons.empty());
}

TEST(RuntimeStatusExportPortTest, EstopStateExportsEstopResetCapability) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Estop";
    supervision_port->next_snapshot.supervision.state_reason = "interlock_estop";
    supervision_port->next_snapshot.io.estop = true;
    supervision_port->next_snapshot.io.estop_known = true;
    supervision_port->next_snapshot.effective_interlocks.estop_active = true;
    supervision_port->next_snapshot.effective_interlocks.estop_known = true;
    supervision_port->next_snapshot.io.door = false;
    supervision_port->next_snapshot.io.door_known = true;
    supervision_port->next_snapshot.effective_interlocks.door_open_active = false;
    supervision_port->next_snapshot.effective_interlocks.door_open_known = true;

    RuntimeStatusExportPort port(supervision_port);
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_FALSE(result.Value().action_capabilities.motion_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.active_job_present);
    EXPECT_TRUE(result.Value().action_capabilities.estop_reset_permitted);
}

TEST(RuntimeStatusExportPortTest, ManualDispenserResumeCapabilityRequiresPausedValveState) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Idle";
    supervision_port->next_snapshot.supervision.state_reason = "ready";
    SetKnownSafeGateSignals(supervision_port->next_snapshot);

    auto dispenser_reader_state = std::make_shared<FakeDispenserReaderState>();
    dispenser_reader_state->dispenser_state.status = DispenserValveStatus::Paused;
    dispenser_reader_state->dispenser_state.completedCount = 1;
    dispenser_reader_state->dispenser_state.totalCount = 2;
    dispenser_reader_state->dispenser_state.progress = 50.0f;

    RuntimeStatusExportPort port(
        supervision_port,
        {},
        BuildDispenserStatusReader(dispenser_reader_state),
        {});
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().action_capabilities.motion_commands_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.manual_output_commands_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_TRUE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.active_job_present);
    EXPECT_FALSE(result.Value().action_capabilities.estop_reset_permitted);
}

TEST(RuntimeStatusExportPortTest, ManualDispenserPauseCapabilityRequiresRunningValveState) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Idle";
    supervision_port->next_snapshot.supervision.state_reason = "ready";
    SetKnownSafeGateSignals(supervision_port->next_snapshot);

    auto dispenser_reader_state = std::make_shared<FakeDispenserReaderState>();
    dispenser_reader_state->dispenser_state.status = DispenserValveStatus::Running;
    dispenser_reader_state->dispenser_state.completedCount = 1;
    dispenser_reader_state->dispenser_state.totalCount = 2;
    dispenser_reader_state->dispenser_state.progress = 50.0f;

    RuntimeStatusExportPort port(
        supervision_port,
        {},
        BuildDispenserStatusReader(dispenser_reader_state),
        {});
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().action_capabilities.manual_dispenser_pause_permitted);
    EXPECT_FALSE(result.Value().action_capabilities.manual_dispenser_resume_permitted);
}

TEST(RuntimeStatusExportPortTest, RetainsTerminalJobExecutionUntilNextJobStarts) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.active_job_id = "job-terminal";
    supervision_port->next_snapshot.active_job_state = "running";
    supervision_port->next_snapshot.supervision.current_state = "Running";
    supervision_port->next_snapshot.supervision.state_reason = "executing";

    auto job_execution_reader_state = std::make_shared<FakeJobExecutionReaderState>();
    job_execution_reader_state->snapshots_by_job_id.emplace(
        "job-terminal",
        BuildJobExecutionSnapshot("job-terminal", "running", 0, 20));
    job_execution_reader_state->snapshots_by_job_id.emplace(
        "job-next",
        BuildJobExecutionSnapshot("job-next", "running", 0, 10));

    RuntimeStatusExportPort port(
        supervision_port,
        {},
        {},
        BuildJobExecutionStatusReader(job_execution_reader_state));

    auto first_result = port.ReadSnapshot();
    ASSERT_TRUE(first_result.IsSuccess()) << first_result.GetError().GetMessage();
    EXPECT_EQ(first_result.Value().job_execution.job_id, "job-terminal");
    EXPECT_EQ(first_result.Value().job_execution.state, "running");
    EXPECT_EQ(job_execution_reader_state->read_count, 1);

    supervision_port->next_snapshot.active_job_id.clear();
    supervision_port->next_snapshot.active_job_state.clear();
    job_execution_reader_state->snapshots_by_job_id["job-terminal"] =
        BuildJobExecutionSnapshot("job-terminal", "completed", 2, 100);

    auto second_result = port.ReadSnapshot();
    ASSERT_TRUE(second_result.IsSuccess()) << second_result.GetError().GetMessage();
    EXPECT_EQ(second_result.Value().job_execution.job_id, "job-terminal");
    EXPECT_EQ(second_result.Value().job_execution.state, "completed");
    EXPECT_EQ(job_execution_reader_state->read_count, 2);

    auto third_result = port.ReadSnapshot();
    ASSERT_TRUE(third_result.IsSuccess()) << third_result.GetError().GetMessage();
    EXPECT_EQ(third_result.Value().job_execution.job_id, "job-terminal");
    EXPECT_EQ(third_result.Value().job_execution.state, "completed");
    EXPECT_EQ(job_execution_reader_state->read_count, 2);

    supervision_port->next_snapshot.active_job_id = "job-next";
    supervision_port->next_snapshot.active_job_state = "running";

    auto fourth_result = port.ReadSnapshot();
    ASSERT_TRUE(fourth_result.IsSuccess()) << fourth_result.GetError().GetMessage();
    EXPECT_EQ(fourth_result.Value().job_execution.job_id, "job-next");
    EXPECT_EQ(fourth_result.Value().job_execution.state, "running");
    EXPECT_EQ(job_execution_reader_state->read_count, 3);
}

}  // namespace
