#include "services/motion/SoftLimitMonitorService.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <vector>

namespace {

using SoftLimitMonitorConfig = Siligen::Application::Services::Motion::SoftLimitMonitorConfig;
using SoftLimitMonitorService = Siligen::Application::Services::Motion::SoftLimitMonitorService;
using SoftLimitTriggeredEvent = Siligen::Domain::System::Ports::SoftLimitTriggeredEvent;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventHandler = Siligen::Domain::System::Ports::EventHandler;
using EventType = Siligen::Domain::System::Ports::EventType;
using IEventPublisherPort = Siligen::Domain::System::Ports::IEventPublisherPort;
using IMotionStatePort = Siligen::Domain::Motion::Ports::IMotionStatePort;
using IPositionControlPort = Siligen::Domain::Motion::Ports::IPositionControlPort;
using ITriggerControllerPort = Siligen::Domain::Dispensing::Ports::ITriggerControllerPort;
using IMachineExecutionStatePort = Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort;
using MachineExecutionSnapshot = Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot;
using MotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using TriggerConfig = Siligen::Domain::Dispensing::Ports::TriggerConfig;
using TriggerStatus = Siligen::Domain::Dispensing::Ports::TriggerStatus;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using Point2D = Siligen::Shared::Types::Point2D;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using float32 = Siligen::Shared::Types::float32;
using int32 = Siligen::Shared::Types::int32;

template <typename T>
using Result = Siligen::Shared::Types::Result<T>;

class StubMotionStatePort final : public IMotionStatePort {
   public:
    Result<Point2D> GetCurrentPosition() const override {
        return Result<Point2D>::Success(Point2D{x_status.position.x, y_status.position.y});
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        return Result<float32>::Success(axis == LogicalAxisId::Y ? y_status.axis_position_mm : x_status.axis_position_mm);
    }

    Result<float32> GetAxisVelocity(LogicalAxisId) const override {
        return Result<float32>::Success(0.0f);
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        return Result<MotionStatus>::Success(axis == LogicalAxisId::Y ? y_status : x_status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId) const override {
        return Result<bool>::Success(false);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId) const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        return Result<std::vector<MotionStatus>>::Success({x_status, y_status});
    }

    MotionStatus x_status{};
    MotionStatus y_status{};
};

class CapturingEventPublisher final : public IEventPublisherPort {
   public:
    ResultVoid Publish(const DomainEvent& event) override {
        ++publish_calls;
        if (publish_error.has_value()) {
            return ResultVoid::Failure(publish_error.value());
        }
        if (const auto* typed_event = dynamic_cast<const SoftLimitTriggeredEvent*>(&event)) {
            soft_limit_events.push_back(*typed_event);
        }
        return ResultVoid::Success();
    }

    ResultVoid PublishAsync(const DomainEvent& event) override {
        return Publish(event);
    }

    Result<int32> Subscribe(EventType, EventHandler) override {
        return Result<int32>::Success(1);
    }

    ResultVoid Unsubscribe(int32) override {
        return ResultVoid::Success();
    }

    ResultVoid UnsubscribeAll(EventType) override {
        return ResultVoid::Success();
    }

    Result<std::vector<std::shared_ptr<const DomainEvent>>> GetEventHistory(EventType, int32 = 100) const override {
        return Result<std::vector<std::shared_ptr<const DomainEvent>>>::Success({});
    }

    ResultVoid ClearEventHistory() override {
        soft_limit_events.clear();
        return ResultVoid::Success();
    }

    int publish_calls = 0;
    std::optional<Error> publish_error;
    std::vector<SoftLimitTriggeredEvent> soft_limit_events;
};

class CountingPositionControlPort final : public IPositionControlPort {
   public:
    ResultVoid MoveToPosition(const Point2D&, float32) override { return ResultVoid::Success(); }
    ResultVoid MoveAxisToPosition(LogicalAxisId, float32, float32) override { return ResultVoid::Success(); }
    ResultVoid RelativeMove(LogicalAxisId, float32, float32) override { return ResultVoid::Success(); }
    ResultVoid SynchronizedMove(const std::vector<MotionCommand>&) override { return ResultVoid::Success(); }
    ResultVoid StopAxis(LogicalAxisId, bool) override { return ResultVoid::Success(); }

    ResultVoid StopAllAxes(bool) override {
        ++stop_all_axes_calls;
        if (stop_all_axes_error.has_value()) {
            return ResultVoid::Failure(stop_all_axes_error.value());
        }
        return ResultVoid::Success();
    }

    ResultVoid EmergencyStop() override {
        ++emergency_stop_calls;
        if (emergency_stop_error.has_value()) {
            return ResultVoid::Failure(emergency_stop_error.value());
        }
        return ResultVoid::Success();
    }

    ResultVoid RecoverFromEmergencyStop() override { return ResultVoid::Success(); }
    ResultVoid WaitForMotionComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }

    int stop_all_axes_calls = 0;
    int emergency_stop_calls = 0;
    std::optional<Error> stop_all_axes_error;
    std::optional<Error> emergency_stop_error;
};

class CountingTriggerControllerPort final : public ITriggerControllerPort {
   public:
    ResultVoid ConfigureTrigger(const TriggerConfig&) override { return ResultVoid::Success(); }
    Result<TriggerConfig> GetTriggerConfig() const override { return Result<TriggerConfig>::Success(TriggerConfig{}); }
    ResultVoid SetSingleTrigger(LogicalAxisId, float32, int32) override { return ResultVoid::Success(); }
    ResultVoid SetContinuousTrigger(LogicalAxisId, float32, float32, float32, int32) override { return ResultVoid::Success(); }
    ResultVoid SetRangeTrigger(LogicalAxisId, float32, float32, int32) override { return ResultVoid::Success(); }
    ResultVoid SetSequenceTrigger(LogicalAxisId, const std::vector<float32>&, int32) override { return ResultVoid::Success(); }
    ResultVoid EnableTrigger(LogicalAxisId) override { return ResultVoid::Success(); }

    ResultVoid DisableTrigger(LogicalAxisId axis) override {
        ++disable_trigger_calls;
        last_disabled_axis = axis;
        return ResultVoid::Success();
    }

    ResultVoid ClearTrigger(LogicalAxisId) override { return ResultVoid::Success(); }
    Result<TriggerStatus> GetTriggerStatus(LogicalAxisId) const override { return Result<TriggerStatus>::Success(TriggerStatus{}); }
    Result<bool> IsTriggerEnabled(LogicalAxisId) const override { return Result<bool>::Success(false); }
    Result<int32> GetTriggerCount(LogicalAxisId) const override { return Result<int32>::Success(0); }

    int disable_trigger_calls = 0;
    LogicalAxisId last_disabled_axis = LogicalAxisId::INVALID;
};

class CountingMachineExecutionStatePort final : public IMachineExecutionStatePort {
   public:
    Result<MachineExecutionSnapshot> ReadSnapshot() const override {
        return Result<MachineExecutionSnapshot>::Success(snapshot);
    }

    ResultVoid ClearPendingTasks() override {
        ++clear_pending_tasks_calls;
        snapshot.pending_task_count = 0;
        return ResultVoid::Success();
    }

    ResultVoid TransitionToEmergencyStop() override {
        ++transition_to_emergency_stop_calls;
        snapshot.emergency_stopped = true;
        return ResultVoid::Success();
    }

    ResultVoid RecoverToUninitialized() override {
        snapshot.emergency_stopped = false;
        return ResultVoid::Success();
    }

    mutable MachineExecutionSnapshot snapshot{};
    int clear_pending_tasks_calls = 0;
    int transition_to_emergency_stop_calls = 0;
};

TEST(SoftLimitMonitorServiceTest, PublishesAxisPositionAndTaskIdSnapshot) {
    auto motion_state_port = std::make_shared<StubMotionStatePort>();
    motion_state_port->y_status.soft_limit_negative = true;
    motion_state_port->y_status.position = Point2D{11.0f, 22.0f};
    motion_state_port->y_status.axis_position_mm = 33.0f;

    auto event_port = std::make_shared<CapturingEventPublisher>();
    auto position_control_port = std::make_shared<CountingPositionControlPort>();

    SoftLimitMonitorConfig config;
    config.monitored_axes = {LogicalAxisId::Y};
    config.auto_stop_on_trigger = true;
    config.emergency_stop_on_trigger = false;

    SoftLimitMonitorService service(motion_state_port, event_port, position_control_port, config);
    service.SetCurrentTaskId("task-42");
    service.SetActiveJobIdProvider([]() {
        return std::string("job-7");
    });

    float32 callback_position = 0.0f;
    service.SetTriggerCallback([&callback_position](LogicalAxisId, float32 position, bool) {
        callback_position = position;
    });

    auto check_result = service.CheckSoftLimits();
    ASSERT_TRUE(check_result.IsSuccess()) << check_result.GetError().GetMessage();
    EXPECT_TRUE(check_result.Value());

    ASSERT_EQ(event_port->soft_limit_events.size(), 1U);
    const auto& event = event_port->soft_limit_events.front();
    EXPECT_EQ(event.axis, LogicalAxisId::Y);
    EXPECT_EQ(event.task_id, "task-42");
    EXPECT_EQ(event.job_id, "job-7");
    EXPECT_FLOAT_EQ(event.position, 33.0f);
    EXPECT_FALSE(event.is_positive_limit);
    EXPECT_EQ(event.reason_code, "soft_limit_negative");
    EXPECT_EQ(event.error_code, 1104);
    EXPECT_FLOAT_EQ(callback_position, 33.0f);
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 1);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 0);
}

TEST(SoftLimitMonitorServiceTest, UsesEmergencySafetyPathAndSuppressesDuplicateSoftLimitEvents) {
    auto motion_state_port = std::make_shared<StubMotionStatePort>();
    motion_state_port->y_status.soft_limit_negative = true;
    motion_state_port->y_status.axis_position_mm = 33.0f;

    auto event_port = std::make_shared<CapturingEventPublisher>();
    auto position_control_port = std::make_shared<CountingPositionControlPort>();
    auto trigger_port = std::make_shared<CountingTriggerControllerPort>();
    auto machine_execution_state_port = std::make_shared<CountingMachineExecutionStatePort>();
    machine_execution_state_port->snapshot.pending_task_count = 3;

    SoftLimitMonitorConfig config;
    config.monitored_axes = {LogicalAxisId::Y};
    config.auto_stop_on_trigger = true;
    config.emergency_stop_on_trigger = true;

    SoftLimitMonitorService service(
        motion_state_port,
        event_port,
        position_control_port,
        trigger_port,
        machine_execution_state_port,
        config);

    auto first_check_result = service.CheckSoftLimits();
    ASSERT_TRUE(first_check_result.IsSuccess()) << first_check_result.GetError().GetMessage();
    EXPECT_TRUE(first_check_result.Value());
    ASSERT_EQ(event_port->soft_limit_events.size(), 1U);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 1);
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 1);
    EXPECT_EQ(trigger_port->disable_trigger_calls, 1);
    EXPECT_EQ(trigger_port->last_disabled_axis, LogicalAxisId::X);
    EXPECT_EQ(machine_execution_state_port->clear_pending_tasks_calls, 1);
    EXPECT_EQ(machine_execution_state_port->transition_to_emergency_stop_calls, 1);
    EXPECT_TRUE(machine_execution_state_port->snapshot.emergency_stopped);

    auto second_check_result = service.CheckSoftLimits();
    ASSERT_TRUE(second_check_result.IsSuccess()) << second_check_result.GetError().GetMessage();
    EXPECT_FALSE(second_check_result.Value());
    EXPECT_EQ(event_port->soft_limit_events.size(), 1U);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 1);

    motion_state_port->y_status.soft_limit_negative = false;
    auto clear_check_result = service.CheckSoftLimits();
    ASSERT_TRUE(clear_check_result.IsSuccess()) << clear_check_result.GetError().GetMessage();
    EXPECT_FALSE(clear_check_result.Value());

    motion_state_port->y_status.soft_limit_negative = true;
    auto retrigger_check_result = service.CheckSoftLimits();
    ASSERT_TRUE(retrigger_check_result.IsSuccess()) << retrigger_check_result.GetError().GetMessage();
    EXPECT_TRUE(retrigger_check_result.Value());
    EXPECT_EQ(event_port->soft_limit_events.size(), 2U);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 2);
}

TEST(SoftLimitMonitorServiceTest, PublishFailureIsReturnedButCallbackAndStopStillRun) {
    auto motion_state_port = std::make_shared<StubMotionStatePort>();
    motion_state_port->y_status.soft_limit_negative = true;
    motion_state_port->y_status.position = Point2D{11.0f, 22.0f};
    motion_state_port->y_status.axis_position_mm = 33.0f;

    auto event_port = std::make_shared<CapturingEventPublisher>();
    event_port->publish_error = Error(ErrorCode::UNKNOWN_ERROR, "publish failed", "test");
    auto position_control_port = std::make_shared<CountingPositionControlPort>();

    SoftLimitMonitorConfig config;
    config.monitored_axes = {LogicalAxisId::Y};
    config.auto_stop_on_trigger = true;
    config.emergency_stop_on_trigger = false;

    SoftLimitMonitorService service(motion_state_port, event_port, position_control_port, config);

    float32 callback_position = 0.0f;
    service.SetTriggerCallback([&callback_position](LogicalAxisId, float32 position, bool) {
        callback_position = position;
    });

    auto check_result = service.CheckSoftLimits();

    ASSERT_TRUE(check_result.IsError());
    EXPECT_EQ(check_result.GetError().GetCode(), ErrorCode::UNKNOWN_ERROR);
    EXPECT_EQ(event_port->publish_calls, 1);
    EXPECT_TRUE(event_port->soft_limit_events.empty());
    EXPECT_FLOAT_EQ(callback_position, 33.0f);
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 1);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 0);
}

TEST(SoftLimitMonitorServiceTest, StopFailureIsReturnedAfterEventPublishAndCallback) {
    auto motion_state_port = std::make_shared<StubMotionStatePort>();
    motion_state_port->y_status.soft_limit_negative = true;
    motion_state_port->y_status.position = Point2D{11.0f, 22.0f};
    motion_state_port->y_status.axis_position_mm = 33.0f;

    auto event_port = std::make_shared<CapturingEventPublisher>();
    auto position_control_port = std::make_shared<CountingPositionControlPort>();
    position_control_port->stop_all_axes_error =
        Error(ErrorCode::HARDWARE_ERROR, "stop failed", "test");

    SoftLimitMonitorConfig config;
    config.monitored_axes = {LogicalAxisId::Y};
    config.auto_stop_on_trigger = true;
    config.emergency_stop_on_trigger = false;

    SoftLimitMonitorService service(motion_state_port, event_port, position_control_port, config);

    float32 callback_position = 0.0f;
    service.SetTriggerCallback([&callback_position](LogicalAxisId, float32 position, bool) {
        callback_position = position;
    });

    auto check_result = service.CheckSoftLimits();

    ASSERT_TRUE(check_result.IsError());
    EXPECT_EQ(check_result.GetError().GetCode(), ErrorCode::HARDWARE_ERROR);
    ASSERT_EQ(event_port->soft_limit_events.size(), 1U);
    EXPECT_FLOAT_EQ(callback_position, 33.0f);
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 1);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 0);

    auto retry_result = service.CheckSoftLimits();
    ASSERT_TRUE(retry_result.IsError());
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 2);
}

}  // namespace
