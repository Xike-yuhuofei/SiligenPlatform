#include "runtime/status/RuntimeStatusExportPort.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using DispenserValveState = Siligen::Domain::Dispensing::Ports::DispenserValveState;
using DispenserValveStatus = Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using Point2D = Siligen::Shared::Types::Point2D;
using RuntimeDispenserStatusReader = Siligen::Runtime::Service::Status::RuntimeDispenserStatusReader;
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

Siligen::Shared::Types::Error BuildTestError(const std::string& message) {
    return Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, message, "status-test");
}

RuntimeMotionStatusReader BuildMotionStatusReader(const std::shared_ptr<FakeMotionReaderState>& state) {
    RuntimeMotionStatusReader reader;
    reader.read_all_axes_motion_status = [state]() {
        ++state->all_status_reads;
        if (state->fail_all_axes) {
            return Siligen::Shared::Types::Result<std::vector<MotionStatus>>::Failure(BuildTestError("axes failed"));
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
            return Siligen::Shared::Types::Result<SupplyValveStatusDetail>::Failure(BuildTestError("supply failed"));
        }
        return Siligen::Shared::Types::Result<SupplyValveStatusDetail>::Success(state->supply_state);
    };
    return reader;
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
    dispenser_reader_state->supply_state.state = SupplyValveState::Open;

    RuntimeStatusExportPort port(
        supervision_port,
        BuildMotionStatusReader(motion_reader_state),
        BuildDispenserStatusReader(dispenser_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& snapshot = result.Value();
    EXPECT_TRUE(snapshot.connected);
    EXPECT_EQ(snapshot.connection_state, "connected");
    EXPECT_EQ(snapshot.machine_state, "Running");
    EXPECT_EQ(snapshot.machine_state_reason, "executing");
    EXPECT_TRUE(snapshot.interlock_latched);
    EXPECT_EQ(snapshot.active_job_id, "job-42");
    EXPECT_EQ(snapshot.active_job_state, "running");
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
}

TEST(RuntimeStatusExportPortTest, DisconnectedSnapshotSkipsMotionReads) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = false;
    supervision_port->next_snapshot.connection_state = "degraded";

    auto motion_reader_state = std::make_shared<FakeMotionReaderState>();
    RuntimeStatusExportPort port(supervision_port, BuildMotionStatusReader(motion_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_FALSE(result.Value().connected);
    EXPECT_EQ(result.Value().connection_state, "degraded");
    EXPECT_EQ(result.Value().machine_state, "Unknown");
    EXPECT_EQ(result.Value().machine_state_reason, "unknown");
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_EQ(motion_reader_state->all_status_reads, 0);
    EXPECT_EQ(motion_reader_state->current_position_reads, 0);
}

TEST(RuntimeStatusExportPortTest, MissingOptionalReadersStillReturnsAuthoritySnapshot) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Idle";
    supervision_port->next_snapshot.supervision.state_reason = "ready";

    RuntimeStatusExportPort port(supervision_port);
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().machine_state, "Idle");
    EXPECT_EQ(result.Value().machine_state_reason, "ready");
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_FALSE(result.Value().dispenser.valve_open);
    EXPECT_FALSE(result.Value().dispenser.supply_open);
}

TEST(RuntimeStatusExportPortTest, OptionalReaderFailuresDoNotFailSnapshot) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Running";
    supervision_port->next_snapshot.supervision.state_reason = "authoritative";

    auto motion_reader_state = std::make_shared<FakeMotionReaderState>();
    motion_reader_state->fail_all_axes = true;
    motion_reader_state->fail_position = true;

    auto dispenser_reader_state = std::make_shared<FakeDispenserReaderState>();
    dispenser_reader_state->fail_dispenser = true;
    dispenser_reader_state->fail_supply = true;

    RuntimeStatusExportPort port(
        supervision_port,
        BuildMotionStatusReader(motion_reader_state),
        BuildDispenserStatusReader(dispenser_reader_state));
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().machine_state, "Running");
    EXPECT_EQ(result.Value().machine_state_reason, "authoritative");
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_FALSE(result.Value().dispenser.valve_open);
    EXPECT_FALSE(result.Value().dispenser.supply_open);
    EXPECT_EQ(motion_reader_state->all_status_reads, 1);
    EXPECT_EQ(motion_reader_state->current_position_reads, 1);
    EXPECT_EQ(dispenser_reader_state->dispenser_reads, 1);
    EXPECT_EQ(dispenser_reader_state->supply_reads, 1);
}

}  // namespace
