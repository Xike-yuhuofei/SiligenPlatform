#include "domain/safety/domain-services/EmergencyStopService.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "runtime_execution/contracts/system/MachineExecutionSnapshot.h"
#include "shared/types/CMPTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Point2D.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Domain::Motion::DomainServices::MotionControlService;
using Siligen::Domain::Motion::DomainServices::MotionStatusService;
using Siligen::Domain::Safety::DomainServices::EmergencyStopFailureKind;
using Siligen::Domain::Safety::DomainServices::EmergencyStopOptions;
using Siligen::Domain::Safety::DomainServices::EmergencyStopService;
using Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort;
using Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase;
using Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;

class StubMotionControlService final : public MotionControlService {
public:
    Result<void> emergency_stop_result = Result<void>::Success();
    Result<void> stop_all_axes_result = Result<void>::Success();
    Result<void> recover_from_emergency_stop_result = Result<void>::Success();
    int recover_from_emergency_stop_calls = 0;

    Result<void> MoveToPosition(const Point2D& /*position*/, float /*speed*/) override {
        return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, "MoveToPosition not implemented", "StubMotionControl"));
    }

    Result<void> MoveAxisToPosition(Siligen::Shared::Types::LogicalAxisId /*axis_id*/, float /*position*/, float /*speed*/) override {
        return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, "MoveAxisToPosition not implemented", "StubMotionControl"));
    }

    Result<void> MoveRelative(const Point2D& /*offset*/, float /*speed*/) override {
        return Result<void>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, "MoveRelative not implemented", "StubMotionControl"));
    }

    Result<void> StopAllAxes() override {
        return stop_all_axes_result;
    }

    Result<void> EmergencyStop() override {
        return emergency_stop_result;
    }

    Result<void> RecoverFromEmergencyStop() override {
        ++recover_from_emergency_stop_calls;
        return recover_from_emergency_stop_result;
    }
};

class StubMotionStatusService final : public MotionStatusService {
public:
    Result<Point2D> position_result = Result<Point2D>::Success(Point2D{1.0f, 2.0f});

    Result<Point2D> GetCurrentPosition() const override {
        return position_result;
    }

    Result<Siligen::Shared::Types::AxisStatus> GetAxisStatus(Siligen::Shared::Types::LogicalAxisId /*axis_id*/) const override {
        return Result<Siligen::Shared::Types::AxisStatus>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, "GetAxisStatus not implemented", "StubMotionStatus"));
    }

    Result<std::vector<Siligen::Shared::Types::AxisStatus>> GetAllAxisStatus() const override {
        return Result<std::vector<Siligen::Shared::Types::AxisStatus>>::Failure(
            Error(ErrorCode::NOT_IMPLEMENTED, "GetAllAxisStatus not implemented", "StubMotionStatus"));
    }

    Result<bool> IsMoving() const override {
        return Result<bool>::Success(false);
    }

    Result<bool> HasError() const override {
        return Result<bool>::Success(false);
    }
};

class FakeMachineExecutionStatePort final : public IMachineExecutionStatePort {
public:
    Result<MachineExecutionSnapshot> ReadSnapshot() const override {
        return snapshot_result;
    }

    Result<void> ClearPendingTasks() override {
        ++clear_pending_tasks_calls;
        if (clear_pending_tasks_result.IsSuccess()) {
            snapshot.has_pending_tasks = false;
            snapshot.pending_task_count = 0;
            snapshot_result = Result<MachineExecutionSnapshot>::Success(snapshot);
        }
        return clear_pending_tasks_result;
    }

    Result<void> TransitionToEmergencyStop() override {
        ++transition_to_emergency_stop_calls;
        if (transition_to_emergency_stop_result.IsSuccess()) {
            snapshot.phase = MachineExecutionPhase::EmergencyStop;
            snapshot.emergency_stopped = true;
            snapshot.manual_motion_allowed = false;
            snapshot.recent_error_summary = "machine_in_emergency_stop";
            snapshot_result = Result<MachineExecutionSnapshot>::Success(snapshot);
        }
        return transition_to_emergency_stop_result;
    }

    Result<void> RecoverToUninitialized() override {
        ++recover_to_uninitialized_calls;
        if (recover_to_uninitialized_result.IsSuccess()) {
            snapshot.phase = MachineExecutionPhase::Uninitialized;
            snapshot.emergency_stopped = false;
            snapshot.manual_motion_allowed = true;
            snapshot.recent_error_summary.clear();
            snapshot_result = Result<MachineExecutionSnapshot>::Success(snapshot);
        }
        return recover_to_uninitialized_result;
    }

    mutable MachineExecutionSnapshot snapshot{};
    mutable Result<MachineExecutionSnapshot> snapshot_result =
        Result<MachineExecutionSnapshot>::Success(snapshot);
    Result<void> clear_pending_tasks_result = Result<void>::Success();
    Result<void> transition_to_emergency_stop_result = Result<void>::Success();
    Result<void> recover_to_uninitialized_result = Result<void>::Success();
    int clear_pending_tasks_calls = 0;
    int transition_to_emergency_stop_calls = 0;
    int recover_to_uninitialized_calls = 0;
};

}  // namespace

TEST(EmergencyStopServiceTest, ExecuteRecordsActionsAndClearsTasks) {
    auto motion_control = std::make_shared<StubMotionControlService>();
    auto motion_status = std::make_shared<StubMotionStatusService>();
    auto cmp_service = std::make_shared<Siligen::Domain::Dispensing::DomainServices::CMPService>(nullptr, nullptr);
    auto machine_state_port = std::make_shared<FakeMachineExecutionStatePort>();
    machine_state_port->snapshot.phase = MachineExecutionPhase::Ready;
    machine_state_port->snapshot.manual_motion_allowed = true;
    machine_state_port->snapshot.has_pending_tasks = true;
    machine_state_port->snapshot.pending_task_count = 2;
    machine_state_port->snapshot_result = Result<MachineExecutionSnapshot>::Success(machine_state_port->snapshot);

    EmergencyStopService service(motion_control, motion_status, cmp_service, machine_state_port);
    EmergencyStopOptions options;
    auto outcome = service.Execute(options);

    EXPECT_TRUE(outcome.motion_stop.success);
    EXPECT_TRUE(outcome.cmp_disable.success);
    EXPECT_TRUE(outcome.task_queue_clear.success);
    EXPECT_TRUE(outcome.hardware_disable.success);
    EXPECT_TRUE(outcome.state_update.success);

    EXPECT_TRUE(outcome.task_queue_size_available);
    EXPECT_EQ(outcome.task_queue_size, 2);

    EXPECT_TRUE(outcome.stop_position_available);
    EXPECT_FLOAT_EQ(outcome.stop_position.x, 1.0f);
    EXPECT_FLOAT_EQ(outcome.stop_position.y, 2.0f);
    EXPECT_EQ(machine_state_port->clear_pending_tasks_calls, 1);
    EXPECT_EQ(machine_state_port->transition_to_emergency_stop_calls, 1);
    EXPECT_FALSE(machine_state_port->snapshot.has_pending_tasks);
    EXPECT_EQ(machine_state_port->snapshot.pending_task_count, 0);
    EXPECT_TRUE(machine_state_port->snapshot.emergency_stopped);
}

TEST(EmergencyStopServiceTest, ExecuteReportsMissingDependencies) {
    EmergencyStopService service(nullptr, nullptr, nullptr, nullptr);
    EmergencyStopOptions options;
    auto outcome = service.Execute(options);

    EXPECT_EQ(outcome.motion_stop.failure, EmergencyStopFailureKind::DependencyMissing);
    EXPECT_EQ(outcome.cmp_disable.failure, EmergencyStopFailureKind::DependencyMissing);
    EXPECT_EQ(outcome.task_queue_clear.failure, EmergencyStopFailureKind::DependencyMissing);
    EXPECT_EQ(outcome.hardware_disable.failure, EmergencyStopFailureKind::DependencyMissing);
    EXPECT_FALSE(outcome.state_update.attempted);
    EXPECT_FALSE(outcome.stop_position_available);
    EXPECT_STREQ(outcome.motion_stop.error.GetMessage().c_str(), "Motion service not initialized");
}

TEST(EmergencyStopServiceTest, RecoverFromEmergencyStopValidatesState) {
    auto motion_control = std::make_shared<StubMotionControlService>();
    auto machine_state_port = std::make_shared<FakeMachineExecutionStatePort>();
    machine_state_port->snapshot.phase = MachineExecutionPhase::Ready;
    machine_state_port->snapshot.manual_motion_allowed = true;
    machine_state_port->snapshot_result = Result<MachineExecutionSnapshot>::Success(machine_state_port->snapshot);
    EmergencyStopService service(motion_control, nullptr, nullptr, machine_state_port);

    auto invalid_result = service.RecoverFromEmergencyStop();
    EXPECT_TRUE(invalid_result.IsError());
    EXPECT_EQ(invalid_result.GetError().GetCode(), ErrorCode::INVALID_STATE);

    machine_state_port->snapshot.phase = MachineExecutionPhase::EmergencyStop;
    machine_state_port->snapshot.emergency_stopped = true;
    machine_state_port->snapshot.manual_motion_allowed = false;
    machine_state_port->snapshot_result = Result<MachineExecutionSnapshot>::Success(machine_state_port->snapshot);
    auto check_result = service.IsInEmergencyStop();
    EXPECT_TRUE(check_result.IsSuccess());
    EXPECT_TRUE(check_result.Value());

    auto recover_result = service.RecoverFromEmergencyStop();
    EXPECT_TRUE(recover_result.IsSuccess());
    EXPECT_EQ(machine_state_port->recover_to_uninitialized_calls, 1);
    EXPECT_EQ(machine_state_port->snapshot.phase, MachineExecutionPhase::Uninitialized);
    EXPECT_EQ(motion_control->recover_from_emergency_stop_calls, 1);
}

TEST(EmergencyStopServiceTest, RecoverFromEmergencyStopKeepsEmergencyStopStateWhenMotionRecoveryFails) {
    auto motion_control = std::make_shared<StubMotionControlService>();
    motion_control->recover_from_emergency_stop_result =
        Result<void>::Failure(Error(ErrorCode::MOTION_ERROR, "recover failed", "StubMotionControl"));
    auto machine_state_port = std::make_shared<FakeMachineExecutionStatePort>();
    machine_state_port->snapshot.phase = MachineExecutionPhase::EmergencyStop;
    machine_state_port->snapshot.emergency_stopped = true;
    machine_state_port->snapshot.manual_motion_allowed = false;
    machine_state_port->snapshot_result = Result<MachineExecutionSnapshot>::Success(machine_state_port->snapshot);

    EmergencyStopService service(motion_control, nullptr, nullptr, machine_state_port);

    auto recover_result = service.RecoverFromEmergencyStop();

    EXPECT_TRUE(recover_result.IsError());
    EXPECT_EQ(recover_result.GetError().GetCode(), ErrorCode::MOTION_ERROR);
    EXPECT_EQ(machine_state_port->recover_to_uninitialized_calls, 0);
    EXPECT_TRUE(machine_state_port->snapshot.emergency_stopped);
    EXPECT_EQ(motion_control->recover_from_emergency_stop_calls, 1);
}
