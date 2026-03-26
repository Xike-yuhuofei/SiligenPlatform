#include "domain/safety/domain-services/EmergencyStopService.h"
#include "domain/machine/aggregates/DispenserModel.h"
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
using Siligen::Domain::Machine::Aggregates::Legacy::DispensingTask;
using Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel;

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

DispensingTask BuildTask(const std::string& id) {
    DispensingTask task;
    task.task_id = id;
    task.movement_speed = 1.0f;
    task.path = {Point2D{0.0f, 0.0f}};

    Siligen::Shared::Types::CMPConfiguration cmp;
    cmp.trigger_points.push_back(Siligen::Shared::Types::CMPTriggerPoint());
    task.cmp_config = cmp;

    return task;
}

}  // namespace

TEST(EmergencyStopServiceTest, ExecuteRecordsActionsAndClearsTasks) {
    auto motion_control = std::make_shared<StubMotionControlService>();
    auto motion_status = std::make_shared<StubMotionStatusService>();
    auto cmp_service = std::make_shared<Siligen::Domain::Dispensing::DomainServices::CMPService>(nullptr, nullptr);
    auto model = std::make_shared<DispenserModel>();

    EXPECT_TRUE(model->AddTask(BuildTask("t1")).IsSuccess());
    EXPECT_TRUE(model->AddTask(BuildTask("t2")).IsSuccess());

    EmergencyStopService service(motion_control, motion_status, cmp_service, model);
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

    auto size_result = model->GetTaskQueueSize();
    EXPECT_TRUE(size_result.IsSuccess());
    EXPECT_EQ(size_result.Value(), 0);
    EXPECT_EQ(model->GetState(), Siligen::DispenserState::EMERGENCY_STOP);
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
    auto model = std::make_shared<DispenserModel>();
    auto motion_control = std::make_shared<StubMotionControlService>();
    EmergencyStopService service(motion_control, nullptr, nullptr, model);

    auto invalid_result = service.RecoverFromEmergencyStop();
    EXPECT_TRUE(invalid_result.IsError());
    EXPECT_EQ(invalid_result.GetError().GetCode(), ErrorCode::INVALID_STATE);

    EXPECT_TRUE(model->SetState(Siligen::DispenserState::EMERGENCY_STOP).IsSuccess());
    auto check_result = service.IsInEmergencyStop();
    EXPECT_TRUE(check_result.IsSuccess());
    EXPECT_TRUE(check_result.Value());

    auto recover_result = service.RecoverFromEmergencyStop();
    EXPECT_TRUE(recover_result.IsSuccess());
    EXPECT_EQ(model->GetState(), Siligen::DispenserState::UNINITIALIZED);
    EXPECT_EQ(motion_control->recover_from_emergency_stop_calls, 1);
}

TEST(EmergencyStopServiceTest, RecoverFromEmergencyStopKeepsEmergencyStopStateWhenMotionRecoveryFails) {
    auto motion_control = std::make_shared<StubMotionControlService>();
    motion_control->recover_from_emergency_stop_result =
        Result<void>::Failure(Error(ErrorCode::MOTION_ERROR, "recover failed", "StubMotionControl"));
    auto model = std::make_shared<DispenserModel>();
    ASSERT_TRUE(model->SetState(Siligen::DispenserState::EMERGENCY_STOP).IsSuccess());

    EmergencyStopService service(motion_control, nullptr, nullptr, model);

    auto recover_result = service.RecoverFromEmergencyStop();

    EXPECT_TRUE(recover_result.IsError());
    EXPECT_EQ(recover_result.GetError().GetCode(), ErrorCode::MOTION_ERROR);
    EXPECT_EQ(model->GetState(), Siligen::DispenserState::EMERGENCY_STOP);
    EXPECT_EQ(motion_control->recover_from_emergency_stop_calls, 1);
}
