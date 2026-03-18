#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"

#include "gtest/gtest.h"

namespace {

using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IIOControlPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::IOStatus;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;

class FakeMotionStatePort final : public IMotionStatePort {
   public:
    Result<Point2D> GetCurrentPosition() const override { return Result<Point2D>::Success(status.position); }
    Result<float32> GetAxisPosition(LogicalAxisId /*axis*/) const override {
        return Result<float32>::Success(status.position.x);
    }
    Result<float32> GetAxisVelocity(LogicalAxisId /*axis*/) const override {
        return Result<float32>::Success(status.velocity);
    }
    Result<MotionStatus> GetAxisStatus(LogicalAxisId /*axis*/) const override {
        return Result<MotionStatus>::Success(status);
    }
    Result<bool> IsAxisMoving(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(status.state == MotionState::MOVING);
    }
    Result<bool> IsAxisInPosition(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(status.in_position);
    }
    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({status});
    }

    MotionStatus status{};
};

class FakeIOControlPort final : public IIOControlPort {
   public:
    Result<IOStatus> ReadDigitalInput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        status.signal_active = false;
        return Result<IOStatus>::Success(status);
    }
    Result<IOStatus> ReadDigitalOutput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        status.signal_active = false;
        return Result<IOStatus>::Success(status);
    }
    Result<void> WriteDigitalOutput(int16 /*channel*/, bool /*value*/) override {
        return Result<void>::Success();
    }
    Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) override {
        last_limit_axis = axis;
        last_limit_positive = positive;
        return Result<bool>::Success(limit_status);
    }
    Result<bool> ReadServoAlarm(LogicalAxisId /*axis*/) override {
        return Result<bool>::Success(false);
    }

    LogicalAxisId last_limit_axis = LogicalAxisId::INVALID;
    bool last_limit_positive = false;
    bool limit_status = false;
};

class FakeHomingPort final : public IHomingPort {
   public:
    Result<void> HomeAxis(LogicalAxisId /*axis*/) override { return Result<void>::Success(); }
    Result<void> StopHoming(LogicalAxisId /*axis*/) override { return Result<void>::Success(); }
    Result<void> ResetHomingState(LogicalAxisId /*axis*/) override { return Result<void>::Success(); }
    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        status.state = homing_state;
        return Result<HomingStatus>::Success(status);
    }
    Result<bool> IsAxisHomed(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(homing_state == HomingState::HOMED);
    }
    Result<bool> IsHomingInProgress(LogicalAxisId /*axis*/) const override {
        return Result<bool>::Success(homing_state == HomingState::HOMING);
    }
    Result<void> WaitForHomingComplete(LogicalAxisId /*axis*/, int32 /*timeout_ms*/ = 30000) override {
        return Result<void>::Success();
    }

    HomingState homing_state = HomingState::NOT_HOMED;
};

TEST(MotionMonitoringUseCaseTest, OverlaysHomingStateOntoRuntimeStatus) {
    auto state_port = std::make_shared<FakeMotionStatePort>();
    auto io_port = std::make_shared<FakeIOControlPort>();
    auto homing_port = std::make_shared<FakeHomingPort>();

    state_port->status.state = MotionState::IDLE;
    homing_port->homing_state = HomingState::HOMED;

    MotionMonitoringUseCase use_case(state_port, io_port, homing_port);
    auto result = use_case.GetAxisMotionStatus(LogicalAxisId::X);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value().state, MotionState::HOMED);
}

TEST(MotionMonitoringUseCaseTest, ReadsLimitStatusThroughUnifiedIoPort) {
    auto state_port = std::make_shared<FakeMotionStatePort>();
    auto io_port = std::make_shared<FakeIOControlPort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    io_port->limit_status = true;

    MotionMonitoringUseCase use_case(state_port, io_port, homing_port);
    auto result = use_case.ReadLimitStatus(LogicalAxisId::Y, true);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_TRUE(result.Value());
    EXPECT_EQ(io_port->last_limit_axis, LogicalAxisId::Y);
    EXPECT_TRUE(io_port->last_limit_positive);
}

}  // namespace
