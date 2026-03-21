#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"

#include "gtest/gtest.h"

namespace {

using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IIOControlPort;
using Siligen::Domain::Motion::Ports::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::IOStatus;
using Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

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

class FakeInterpolationPort final : public IInterpolationPort {
   public:
    Result<void> ConfigureCoordinateSystem(int16, const CoordinateSystemConfig&) override { return Result<void>::Success(); }
    Result<void> AddInterpolationData(int16, const InterpolationData&) override { return Result<void>::Success(); }
    Result<void> ClearInterpolationBuffer(int16) override { return Result<void>::Success(); }
    Result<void> FlushInterpolationData(int16) override { return Result<void>::Success(); }
    Result<void> StartCoordinateSystemMotion(uint32) override { return Result<void>::Success(); }
    Result<void> StopCoordinateSystemMotion(uint32) override { return Result<void>::Success(); }
    Result<void> SetCoordinateSystemVelocityOverride(int16, float32) override { return Result<void>::Success(); }
    Result<void> EnableCoordinateSystemSCurve(int16, float32) override { return Result<void>::Success(); }
    Result<void> DisableCoordinateSystemSCurve(int16) override { return Result<void>::Success(); }
    Result<void> SetConstLinearVelocityMode(int16, bool, uint32) override { return Result<void>::Success(); }
    Result<uint32> GetInterpolationBufferSpace(int16) const override { return Result<uint32>::Success(buffer_space); }
    Result<uint32> GetLookAheadBufferSpace(int16) const override { return Result<uint32>::Success(lookahead_space); }
    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16) const override {
        return Result<CoordinateSystemStatus>::Success(status);
    }

    CoordinateSystemStatus status{};
    uint32 buffer_space = 0;
    uint32 lookahead_space = 0;
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

TEST(MotionMonitoringUseCaseTest, ExposesCoordinateSystemDiagnosticsWhenInterpolationPortAvailable) {
    auto state_port = std::make_shared<FakeMotionStatePort>();
    auto io_port = std::make_shared<FakeIOControlPort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    interpolation_port->status.state = CoordinateSystemState::MOVING;
    interpolation_port->status.is_moving = true;
    interpolation_port->status.remaining_segments = 7;
    interpolation_port->status.current_velocity = 12.5f;
    interpolation_port->status.raw_status_word = 0x21;
    interpolation_port->status.raw_segment = 7;
    interpolation_port->status.mc_status_ret = 0;
    interpolation_port->buffer_space = 42;
    interpolation_port->lookahead_space = 3;

    MotionMonitoringUseCase use_case(state_port, io_port, homing_port, interpolation_port);
    auto coord_result = use_case.GetCoordinateSystemStatus(1);
    auto buffer_result = use_case.GetInterpolationBufferSpace(1);
    auto lookahead_result = use_case.GetLookAheadBufferSpace(1);

    ASSERT_TRUE(coord_result.IsSuccess());
    EXPECT_EQ(coord_result.Value().state, CoordinateSystemState::MOVING);
    EXPECT_TRUE(coord_result.Value().is_moving);
    EXPECT_EQ(coord_result.Value().remaining_segments, 7U);
    EXPECT_EQ(coord_result.Value().raw_status_word, 0x21);
    ASSERT_TRUE(buffer_result.IsSuccess());
    EXPECT_EQ(buffer_result.Value(), 42U);
    ASSERT_TRUE(lookahead_result.IsSuccess());
    EXPECT_EQ(lookahead_result.Value(), 3U);
}

}  // namespace
