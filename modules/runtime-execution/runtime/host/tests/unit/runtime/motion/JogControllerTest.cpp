#include "runtime_execution/application/services/motion/JogController.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Error.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

namespace {

using Siligen::Domain::Motion::Ports::IJogControlPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::RuntimeExecution::Application::Services::Motion::JogController;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;

class FakeJogControlPort : public IJogControlPort {
public:
    struct StartCall {
        LogicalAxisId axis = LogicalAxisId::INVALID;
        int16 direction = 0;
        float32 velocity = 0.0f;
    };

    struct StepCall {
        LogicalAxisId axis = LogicalAxisId::INVALID;
        int16 direction = 0;
        float32 distance = 0.0f;
        float32 velocity = 0.0f;
    };

    Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) override {
        ++start_calls;
        last_start.axis = axis;
        last_start.direction = direction;
        last_start.velocity = velocity;
        return start_result;
    }

    Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) override {
        ++step_calls;
        last_step.axis = axis;
        last_step.direction = direction;
        last_step.distance = distance;
        last_step.velocity = velocity;
        return step_result;
    }

    Result<void> StopJog(LogicalAxisId axis) override {
        ++stop_calls;
        last_stop_axis = axis;
        return stop_result;
    }

    Result<void> SetJogParameters(LogicalAxisId, const Siligen::Domain::Motion::Ports::JogParameters&) override {
        return Result<void>::Success();
    }

    int start_calls = 0;
    int step_calls = 0;
    int stop_calls = 0;
    StartCall last_start{};
    StepCall last_step{};
    LogicalAxisId last_stop_axis = LogicalAxisId::INVALID;

    Result<void> start_result = Result<void>::Success();
    Result<void> step_result = Result<void>::Success();
    Result<void> stop_result = Result<void>::Success();
};

class FakeMotionStatePort : public IMotionStatePort {
public:
    Result<Point2D> GetCurrentPosition() const override {
        return Result<Point2D>::Success(Point2D{0.0f, 0.0f});
    }

    Result<float32> GetAxisPosition(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId) const override {
        if (return_error) {
            return Result<MotionStatus>::Failure(status_error);
        }
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({status});
    }

    MotionStatus status{};
    bool return_error = false;
    Error status_error = Error(ErrorCode::HARDWARE_ERROR, "status error", "FakeMotionStatePort");
};

}  // namespace

TEST(JogControllerTest, RejectsInvalidAxis) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::Z, 1, 10.0f);

    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_AXIS);
    EXPECT_EQ(jog_port->start_calls, 0);
}

TEST(JogControllerTest, RejectsNonPositiveVelocity) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, 1, 0.0f);

    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::VELOCITY_LIMIT_EXCEEDED);
}

TEST(JogControllerTest, RejectsNonPositiveDistance) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    JogController controller(jog_port, state_port);

    auto result = controller.StartStepJog(LogicalAxisId::X, 1, 0.0f, 10.0f);

    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
}

TEST(JogControllerTest, NormalizesDirectionBeforeCallingPort) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, 5, 10.0f);

    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(jog_port->start_calls, 1);
    EXPECT_EQ(jog_port->last_start.direction, 1);
}

TEST(JogControllerTest, BlocksJogWhenEmergencyStopActive) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    state_port->status.state = MotionState::ESTOP;
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, 1, 10.0f);

    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::EMERGENCY_STOP_ACTIVATED);
}

TEST(JogControllerTest, BlocksJogWhenHardLimitTriggered) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    state_port->status.hard_limit_positive = true;
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, 1, 10.0f);

    EXPECT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::POSITION_OUT_OF_RANGE);
}

TEST(JogControllerTest, DoesNotInterpretHomeSwitchAsHardLimitInDomainLayer) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    state_port->status.home_signal = true;
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, -1, 10.0f);

    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(jog_port->start_calls, 1);
    EXPECT_EQ(jog_port->last_start.direction, -1);
}

TEST(JogControllerTest, AllowsPositiveJogToEscapeHomeSwitch) {
    auto jog_port = std::make_shared<FakeJogControlPort>();
    auto state_port = std::make_shared<FakeMotionStatePort>();
    state_port->status.home_signal = true;
    JogController controller(jog_port, state_port);

    auto result = controller.StartContinuousJog(LogicalAxisId::X, 1, 10.0f);

    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(jog_port->start_calls, 1);
    EXPECT_EQ(jog_port->last_start.direction, 1);
}
