#include "services/motion/SoftLimitMonitorService.h"

#include <gtest/gtest.h>

#include <memory>
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
using MotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using Point2D = Siligen::Shared::Types::Point2D;
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
        return ResultVoid::Success();
    }

    ResultVoid EmergencyStop() override {
        ++emergency_stop_calls;
        return ResultVoid::Success();
    }

    ResultVoid RecoverFromEmergencyStop() override { return ResultVoid::Success(); }
    ResultVoid WaitForMotionComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }

    int stop_all_axes_calls = 0;
    int emergency_stop_calls = 0;
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
    EXPECT_FLOAT_EQ(event.position, 33.0f);
    EXPECT_FALSE(event.is_positive_limit);
    EXPECT_FLOAT_EQ(callback_position, 33.0f);
    EXPECT_EQ(position_control_port->stop_all_axes_calls, 1);
    EXPECT_EQ(position_control_port->emergency_stop_calls, 0);
}

}  // namespace
