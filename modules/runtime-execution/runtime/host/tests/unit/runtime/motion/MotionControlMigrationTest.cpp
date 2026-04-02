#include "runtime/motion/WorkflowMotionRuntimeServicesProvider.h"
#include "runtime_execution/application/services/motion/JogController.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::UseCases::Motion::MotionControlUseCase;
using Siligen::Application::UseCases::Motion::Homing::EnsureAxesReadyZeroRequest;
using Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionCommand;
using Siligen::Application::UseCases::Motion::Manual::ManualMotionControlUseCase;
using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Domain::Motion::Ports::AxisConfiguration;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::JogParameters;
using Siligen::Domain::Motion::Ports::MotionCommand;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::RuntimeExecution::Host::Motion::WorkflowMotionRuntimeServicesProvider;
using Siligen::RuntimeExecution::Application::Services::Motion::JogController;
using Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort;
using Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort;
using Siligen::RuntimeExecution::Contracts::Motion::IOStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;

int AxisIndex(LogicalAxisId axis) {
    switch (axis) {
        case LogicalAxisId::X:
            return 0;
        case LogicalAxisId::Y:
            return 1;
        default:
            return -1;
    }
}

struct AxisRuntimeState {
    MotionStatus status{};
    HomingState homing_state = HomingState::NOT_HOMED;
    float32 post_home_position_mm = 0.0f;
    bool home_fails = false;
    bool move_fails = false;
};

class FakeMotionRuntimePort final : public IMotionRuntimePort {
   public:
    FakeMotionRuntimePort() {
        for (int index = 0; index < 2; ++index) {
            axes_[index].status.enabled = true;
            axes_[index].status.in_position = true;
            axes_[index].status.state = MotionState::IDLE;
        }
    }

    AxisRuntimeState& Axis(LogicalAxisId axis) { return axes_.at(static_cast<std::size_t>(AxisIndex(axis))); }
    const AxisRuntimeState& Axis(LogicalAxisId axis) const { return axes_.at(static_cast<std::size_t>(AxisIndex(axis))); }

    Result<void> Connect(const std::string&, const std::string&, int16 = 0) override { return Result<void>::Success(); }
    Result<void> Disconnect() override { return Result<void>::Success(); }
    Result<bool> IsConnected() const override { return Result<bool>::Success(connected); }
    Result<void> Reset() override { return Result<void>::Success(); }
    Result<std::string> GetCardInfo() const override { return Result<std::string>::Success("fake-runtime-port"); }

    Result<void> EnableAxis(LogicalAxisId axis) override {
        Axis(axis).status.enabled = true;
        return Result<void>::Success();
    }
    Result<void> DisableAxis(LogicalAxisId axis) override {
        Axis(axis).status.enabled = false;
        return Result<void>::Success();
    }
    Result<bool> IsAxisEnabled(LogicalAxisId axis) const override {
        return Result<bool>::Success(Axis(axis).status.enabled);
    }
    Result<void> ClearAxisStatus(LogicalAxisId axis) override {
        Axis(axis).status.has_error = false;
        return Result<void>::Success();
    }
    Result<void> ClearPosition(LogicalAxisId axis) override {
        return MoveAxisToPosition(axis, 0.0f, 0.0f);
    }
    Result<void> SetAxisVelocity(LogicalAxisId, float32) override { return Result<void>::Success(); }
    Result<void> SetAxisAcceleration(LogicalAxisId, float32) override { return Result<void>::Success(); }
    Result<void> SetSoftLimits(LogicalAxisId, float32, float32) override { return Result<void>::Success(); }
    Result<void> ConfigureAxis(LogicalAxisId, const AxisConfiguration&) override { return Result<void>::Success(); }
    Result<void> SetHardLimits(LogicalAxisId, short, short, short, short) override { return Result<void>::Success(); }
    Result<void> EnableHardLimits(LogicalAxisId, bool, short) override { return Result<void>::Success(); }
    Result<void> SetHardLimitPolarity(LogicalAxisId, short, short) override { return Result<void>::Success(); }

    Result<void> MoveToPosition(const Point2D& position, float32 velocity) override {
        auto x_result = MoveAxisToPosition(LogicalAxisId::X, position.x, velocity);
        if (x_result.IsError()) {
            return x_result;
        }
        return MoveAxisToPosition(LogicalAxisId::Y, position.y, velocity);
    }
    Result<void> MoveAxisToPosition(LogicalAxisId axis, float32 position, float32 velocity) override {
        ++move_calls;
        auto& axis_state = Axis(axis);
        if (axis_state.move_fails) {
            return Result<void>::Failure(Error(ErrorCode::MOTION_ERROR, "Move failed", "FakeMotionRuntimePort"));
        }

        axis_state.status.state = MotionState::IDLE;
        axis_state.status.velocity = 0.0f;
        axis_state.status.axis_position_mm = position;
        axis_state.status.in_position = true;
        if (axis == LogicalAxisId::X) {
            axis_state.status.position.x = position;
        } else if (axis == LogicalAxisId::Y) {
            axis_state.status.position.y = position;
        }
        last_move_velocity = velocity;
        last_axis_move = axis;
        return Result<void>::Success();
    }
    Result<void> RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) override {
        return MoveAxisToPosition(axis, Axis(axis).status.axis_position_mm + distance, velocity);
    }
    Result<void> SynchronizedMove(const std::vector<MotionCommand>& commands) override {
        for (const auto& command : commands) {
            auto result = command.relative
                ? RelativeMove(command.axis, command.position, command.velocity)
                : MoveAxisToPosition(command.axis, command.position, command.velocity);
            if (result.IsError()) {
                return result;
            }
        }
        return Result<void>::Success();
    }
    Result<void> StopAxis(LogicalAxisId axis, bool = false) override {
        Axis(axis).status.velocity = 0.0f;
        Axis(axis).status.state = MotionState::IDLE;
        return Result<void>::Success();
    }
    Result<void> StopAllAxes(bool = false) override {
        for (auto axis : {LogicalAxisId::X, LogicalAxisId::Y}) {
            StopAxis(axis);
        }
        return Result<void>::Success();
    }
    Result<void> EmergencyStop() override { return StopAllAxes(true); }
    Result<void> RecoverFromEmergencyStop() override { return Result<void>::Success(); }
    Result<void> WaitForMotionComplete(LogicalAxisId, int32 = 60000) override { return Result<void>::Success(); }

    Result<Point2D> GetCurrentPosition() const override {
        Point2D position;
        position.x = Axis(LogicalAxisId::X).status.axis_position_mm;
        position.y = Axis(LogicalAxisId::Y).status.axis_position_mm;
        return Result<Point2D>::Success(position);
    }
    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        return Result<float32>::Success(Axis(axis).status.axis_position_mm);
    }
    Result<float32> GetAxisVelocity(LogicalAxisId axis) const override {
        return Result<float32>::Success(Axis(axis).status.velocity);
    }
    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        return Result<MotionStatus>::Success(Axis(axis).status);
    }
    Result<bool> IsAxisMoving(LogicalAxisId axis) const override {
        return Result<bool>::Success(Axis(axis).status.state == MotionState::MOVING);
    }
    Result<bool> IsAxisInPosition(LogicalAxisId axis) const override {
        return Result<bool>::Success(Axis(axis).status.in_position);
    }
    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success(
            {Axis(LogicalAxisId::X).status, Axis(LogicalAxisId::Y).status});
    }

    Result<void> StartJog(LogicalAxisId axis, int16 direction, float32 velocity) override {
        ++start_jog_calls;
        last_jog_axis = axis;
        last_jog_direction = direction;
        last_jog_velocity = velocity;
        Axis(axis).status.state = MotionState::MOVING;
        Axis(axis).status.velocity = velocity;
        return Result<void>::Success();
    }
    Result<void> StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) override {
        ++start_jog_step_calls;
        return RelativeMove(axis, static_cast<float32>(direction) * distance, velocity);
    }
    Result<void> StopJog(LogicalAxisId axis) override {
        ++stop_jog_calls;
        Axis(axis).status.state = MotionState::IDLE;
        Axis(axis).status.velocity = 0.0f;
        return Result<void>::Success();
    }
    Result<void> SetJogParameters(LogicalAxisId, const JogParameters&) override { return Result<void>::Success(); }

    Result<IOStatus> ReadDigitalInput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        return Result<IOStatus>::Success(status);
    }
    Result<IOStatus> ReadDigitalOutput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        return Result<IOStatus>::Success(status);
    }
    Result<void> WriteDigitalOutput(int16, bool) override { return Result<void>::Success(); }
    Result<bool> ReadLimitStatus(LogicalAxisId, bool) override { return Result<bool>::Success(false); }
    Result<bool> ReadServoAlarm(LogicalAxisId) override { return Result<bool>::Success(false); }

    Result<void> HomeAxis(LogicalAxisId axis) override {
        ++home_calls;
        auto& axis_state = Axis(axis);
        if (axis_state.home_fails) {
            return Result<void>::Failure(Error(ErrorCode::HARDWARE_ERROR, "Home failed", "FakeMotionRuntimePort"));
        }
        axis_state.homing_state = HomingState::HOMED;
        axis_state.status.state = MotionState::IDLE;
        axis_state.status.velocity = 0.0f;
        axis_state.status.axis_position_mm = axis_state.post_home_position_mm;
        if (axis == LogicalAxisId::X) {
            axis_state.status.position.x = axis_state.post_home_position_mm;
        } else if (axis == LogicalAxisId::Y) {
            axis_state.status.position.y = axis_state.post_home_position_mm;
        }
        return Result<void>::Success();
    }
    Result<void> StopHoming(LogicalAxisId) override { return Result<void>::Success(); }
    Result<void> ResetHomingState(LogicalAxisId axis) override {
        Axis(axis).homing_state = HomingState::NOT_HOMED;
        return Result<void>::Success();
    }
    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        status.state = Axis(axis).homing_state;
        return Result<HomingStatus>::Success(status);
    }
    Result<bool> IsAxisHomed(LogicalAxisId axis) const override {
        return Result<bool>::Success(Axis(axis).homing_state == HomingState::HOMED);
    }
    Result<bool> IsHomingInProgress(LogicalAxisId axis) const override {
        return Result<bool>::Success(Axis(axis).homing_state == HomingState::HOMING);
    }
    Result<void> WaitForHomingComplete(LogicalAxisId axis, int32 = 30000) override {
        ++wait_for_homing_calls;
        if (Axis(axis).home_fails) {
            return Result<void>::Failure(Error(ErrorCode::TIMEOUT, "Home wait failed", "FakeMotionRuntimePort"));
        }
        return Result<void>::Success();
    }

    int home_calls = 0;
    int wait_for_homing_calls = 0;
    int move_calls = 0;
    int start_jog_calls = 0;
    int start_jog_step_calls = 0;
    int stop_jog_calls = 0;
    float32 last_move_velocity = 0.0f;
    float32 last_jog_velocity = 0.0f;
    int16 last_jog_direction = 0;
    LogicalAxisId last_axis_move = LogicalAxisId::INVALID;
    LogicalAxisId last_jog_axis = LogicalAxisId::INVALID;
    bool connected = true;

   private:
    std::array<AxisRuntimeState, 2> axes_{};
};

TEST(MotionControlMigrationTest, WorkflowMotionRuntimeServicesProviderBuildsControlAndStatusServicesFromM9Port) {
    auto runtime_port = std::make_shared<FakeMotionRuntimePort>();
    WorkflowMotionRuntimeServicesProvider provider;

    const auto bundle = provider.CreateServices(runtime_port);

    ASSERT_NE(bundle.motion_control_service, nullptr);
    ASSERT_NE(bundle.motion_status_service, nullptr);

    auto move_result = bundle.motion_control_service->MoveAxisToPosition(LogicalAxisId::X, 12.5f, 4.0f);
    ASSERT_TRUE(move_result.IsSuccess());
    EXPECT_EQ(runtime_port->move_calls, 1);

    auto axis_status_result = bundle.motion_status_service->GetAxisStatus(LogicalAxisId::X);
    ASSERT_TRUE(axis_status_result.IsSuccess());
    EXPECT_FLOAT_EQ(axis_status_result.Value().current_position, 12.5f);
    EXPECT_FALSE(axis_status_result.Value().HasReachedTarget(0.0f));
}

TEST(MotionControlMigrationTest, MotionControlUseCaseDispatchesJogControlAndMonitoring) {
    auto runtime_port = std::make_shared<FakeMotionRuntimePort>();
    auto jog_controller =
        std::make_shared<JogController>(std::static_pointer_cast<Siligen::Domain::Motion::Ports::IJogControlPort>(runtime_port),
                                        std::static_pointer_cast<Siligen::Domain::Motion::Ports::IMotionStatePort>(runtime_port));
    auto manual_use_case = std::make_shared<ManualMotionControlUseCase>(
        std::static_pointer_cast<Siligen::Domain::Motion::Ports::IPositionControlPort>(runtime_port),
        jog_controller,
        std::static_pointer_cast<Siligen::Domain::Motion::Ports::IHomingPort>(runtime_port));
    auto monitoring_use_case = std::make_shared<MotionMonitoringUseCase>(
        std::static_pointer_cast<Siligen::Domain::Motion::Ports::IMotionStatePort>(runtime_port),
        std::static_pointer_cast<IIOControlPort>(runtime_port),
        std::static_pointer_cast<Siligen::Domain::Motion::Ports::IHomingPort>(runtime_port));
    MotionControlUseCase use_case(nullptr, nullptr, manual_use_case, monitoring_use_case);

    auto jog_result = use_case.StartJog(LogicalAxisId::X, 1, 8.0f);
    ASSERT_TRUE(jog_result.IsSuccess());
    EXPECT_EQ(runtime_port->start_jog_calls, 1);

    auto stop_jog_result = use_case.StopJog(LogicalAxisId::X);
    ASSERT_TRUE(stop_jog_result.IsSuccess());
    EXPECT_EQ(runtime_port->stop_jog_calls, 1);

    ManualMotionCommand command;
    command.axis = LogicalAxisId::X;
    command.position = 15.0f;
    command.velocity = 6.0f;
    auto move_result = use_case.ExecutePointToPointMotion(command, true);
    ASSERT_TRUE(move_result.IsSuccess());
    EXPECT_EQ(runtime_port->move_calls, 1);

    auto status_result = use_case.GetAxisMotionStatus(LogicalAxisId::X);
    ASSERT_TRUE(status_result.IsSuccess());
    EXPECT_FLOAT_EQ(status_result.Value().position.x, 15.0f);

    auto limit_result = use_case.ReadLimitStatus(LogicalAxisId::X, true);
    ASSERT_TRUE(limit_result.IsSuccess());
    EXPECT_FALSE(limit_result.Value());
}

TEST(MotionControlMigrationTest, MotionControlUseCaseReturnsMissingDependencyForHomingEntrypoints) {
    MotionControlUseCase use_case(nullptr, nullptr, nullptr, nullptr);

    HomeAxesRequest home_request;
    home_request.axes = {LogicalAxisId::X};
    auto home_result = use_case.Home(home_request);
    ASSERT_TRUE(home_result.IsError());
    EXPECT_EQ(home_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);

    EnsureAxesReadyZeroRequest ensure_request;
    ensure_request.axes = {LogicalAxisId::X};
    auto ensure_result = use_case.EnsureAxesReadyZero(ensure_request);
    ASSERT_TRUE(ensure_result.IsError());
    EXPECT_EQ(ensure_result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
