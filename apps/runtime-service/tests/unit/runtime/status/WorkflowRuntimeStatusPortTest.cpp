#include "runtime/status/WorkflowRuntimeStatusPort.h"
#include "application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

using MotionControlUseCase = Siligen::Application::UseCases::Motion::MotionControlUseCase;
using RuntimeStatusPort = Siligen::Runtime::Service::Status::WorkflowRuntimeStatusPort;
using RuntimeStatusSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeStatusSnapshot;
using RuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeSupervisionSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using Point2D = Siligen::Shared::Types::Point2D;

class FakeRuntimeSupervisionPort final : public RuntimeSupervisionPort {
   public:
    RuntimeSupervisionSnapshot next_snapshot{};

    Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot> ReadSnapshot() const override {
        return Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot>::Success(next_snapshot);
    }
};

class FakeMotionHomingOperations final : public Siligen::Application::UseCases::Motion::IMotionHomingOperations {
   public:
    Siligen::Shared::Types::Result<Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse> Home(
        const Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest&) override {
        return Siligen::Shared::Types::Result<Siligen::Application::UseCases::Motion::Homing::HomeAxesResponse>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroResponse>
    EnsureAxesReadyZero(const Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroRequest&) override {
        return Siligen::Shared::Types::Result<
            Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroResponse>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<bool> IsAxisHomed(Siligen::Shared::Types::LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }
};

class FakeMotionManualOperations final : public Siligen::Application::UseCases::Motion::IMotionManualOperations {
   public:
    Siligen::Shared::Types::Result<void> ExecutePointToPointMotion(
        const Siligen::Application::UseCases::Motion::Manual::ManualMotionCommand&,
        bool) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<void> StartJog(
        Siligen::Shared::Types::LogicalAxisId,
        int16_t,
        Siligen::Shared::Types::float32) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<void> StopJog(Siligen::Shared::Types::LogicalAxisId) override {
        return Siligen::Shared::Types::Result<void>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }
};

class FakeMotionMonitoringOperations final : public Siligen::Application::UseCases::Motion::IMotionMonitoringOperations {
   public:
    std::vector<MotionStatus> next_statuses;
    Point2D next_position{0.0f, 0.0f};
    mutable int all_status_reads = 0;
    mutable int current_position_reads = 0;

    Siligen::Shared::Types::Result<MotionStatus> GetAxisMotionStatus(Siligen::Shared::Types::LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<MotionStatus>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<std::vector<MotionStatus>> GetAllAxesMotionStatus() const override {
        ++all_status_reads;
        return Siligen::Shared::Types::Result<std::vector<MotionStatus>>::Success(next_statuses);
    }

    Siligen::Shared::Types::Result<Point2D> GetCurrentPosition() const override {
        ++current_position_reads;
        return Siligen::Shared::Types::Result<Point2D>::Success(next_position);
    }

    Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus> GetCoordinateSystemStatus(
        int16_t) const override {
        return Siligen::Shared::Types::Result<Siligen::Domain::Motion::Ports::CoordinateSystemStatus>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<uint32_t> GetInterpolationBufferSpace(int16_t) const override {
        return Siligen::Shared::Types::Result<uint32_t>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<uint32_t> GetLookAheadBufferSpace(int16_t) const override {
        return Siligen::Shared::Types::Result<uint32_t>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<bool> ReadLimitStatus(
        Siligen::Shared::Types::LogicalAxisId,
        bool) const override {
        return Siligen::Shared::Types::Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }

    Siligen::Shared::Types::Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED, "unused"));
    }
};

std::shared_ptr<MotionControlUseCase> BuildMotionControlUseCase(
    const std::shared_ptr<FakeMotionMonitoringOperations>& monitoring) {
    return std::make_shared<MotionControlUseCase>(
        std::make_shared<FakeMotionHomingOperations>(),
        std::make_shared<FakeMotionManualOperations>(),
        monitoring);
}

TEST(WorkflowRuntimeStatusPortTest, MissingSupervisionPortReturnsNotInitializedError) {
    RuntimeStatusPort port(nullptr, nullptr, nullptr);

    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(WorkflowRuntimeStatusPortTest, ConnectedSnapshotIncludesAxesPositionAndCompatDispenserDefaults) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = true;
    supervision_port->next_snapshot.connection_state = "connected";
    supervision_port->next_snapshot.supervision.current_state = "Idle";
    supervision_port->next_snapshot.supervision.state_reason = "idle";

    auto monitoring = std::make_shared<FakeMotionMonitoringOperations>();
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
    monitoring->next_statuses = {x_status, y_status};
    monitoring->next_position = Point2D{12.5f, 8.0f};

    RuntimeStatusPort port(supervision_port, BuildMotionControlUseCase(monitoring), nullptr);
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    const auto& snapshot = result.Value();
    ASSERT_EQ(snapshot.axes.size(), 2U);
    EXPECT_TRUE(snapshot.axes.at("X").homed);
    EXPECT_EQ(snapshot.axes.at("X").homing_state, "homed");
    EXPECT_FALSE(snapshot.axes.at("Y").homed);
    EXPECT_EQ(snapshot.axes.at("Y").homing_state, "searching");
    EXPECT_TRUE(snapshot.has_position);
    EXPECT_DOUBLE_EQ(snapshot.position.x, 12.5);
    EXPECT_DOUBLE_EQ(snapshot.position.y, 8.0);
    EXPECT_FALSE(snapshot.dispenser.valve_open);
    EXPECT_FALSE(snapshot.dispenser.supply_open);
}

TEST(WorkflowRuntimeStatusPortTest, DisconnectedSnapshotSkipsMotionReads) {
    auto supervision_port = std::make_shared<FakeRuntimeSupervisionPort>();
    supervision_port->next_snapshot.connected = false;
    supervision_port->next_snapshot.connection_state = "degraded";

    auto monitoring = std::make_shared<FakeMotionMonitoringOperations>();
    RuntimeStatusPort port(supervision_port, BuildMotionControlUseCase(monitoring), nullptr);
    auto result = port.ReadSnapshot();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().axes.empty());
    EXPECT_FALSE(result.Value().has_position);
    EXPECT_EQ(monitoring->all_status_reads, 0);
    EXPECT_EQ(monitoring->current_position_reads, 0);
}

}  // namespace
