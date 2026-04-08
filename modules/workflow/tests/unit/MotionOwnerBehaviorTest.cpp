#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace {

using Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using Siligen::Application::UseCases::Motion::Runtime::MotionRuntimeAssemblyFactory;
using Siligen::Domain::Diagnostics::Ports::DiagnosticInfo;
using Siligen::Domain::Diagnostics::Ports::DiagnosticLevel;
using Siligen::Domain::Diagnostics::Ports::HealthReport;
using Siligen::Domain::Diagnostics::Ports::HealthState;
using Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort;
using Siligen::Domain::Diagnostics::Ports::PerformanceMetrics;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::System::Ports::DomainEvent;
using Siligen::Domain::System::Ports::EventHandler;
using Siligen::Domain::System::Ports::EventType;
using Siligen::Domain::System::Ports::IEventPublisherPort;
using Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort;
using Siligen::RuntimeExecution::Contracts::Motion::IOStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;

class FakeDiagnosticsPort final : public IDiagnosticsPort {
public:
    Result<HealthReport> GetHealthReport() const override { return Result<HealthReport>::Success({}); }
    Result<HealthState> GetHealthState() const override { return Result<HealthState>::Success(HealthState::HEALTHY); }
    Result<bool> IsSystemHealthy() const override { return Result<bool>::Success(true); }
    Result<std::vector<DiagnosticInfo>> GetDiagnostics(DiagnosticLevel = DiagnosticLevel::INFO) const override {
        return Result<std::vector<DiagnosticInfo>>::Success(diagnostics);
    }
    Result<void> AddDiagnostic(const DiagnosticInfo& info) override {
        diagnostics.push_back(info);
        return Result<void>::Success();
    }
    Result<void> ClearDiagnostics() override {
        diagnostics.clear();
        return Result<void>::Success();
    }
    Result<PerformanceMetrics> GetPerformanceMetrics() const override {
        return Result<PerformanceMetrics>::Success({});
    }
    Result<void> ResetPerformanceMetrics() override { return Result<void>::Success(); }
    Result<bool> IsComponentHealthy(const std::string&) const override { return Result<bool>::Success(true); }
    Result<std::vector<std::string>> GetUnhealthyComponents() const override {
        return Result<std::vector<std::string>>::Success({});
    }

    mutable std::vector<DiagnosticInfo> diagnostics;
};

class FakeEventPublisher final : public IEventPublisherPort {
public:
    Result<void> Publish(const DomainEvent& event) override {
        events.push_back(event);
        return Result<void>::Success();
    }
    Result<void> PublishAsync(const DomainEvent& event) override { return Publish(event); }
    Result<int32> Subscribe(EventType, EventHandler) override { return Result<int32>::Success(1); }
    Result<void> Unsubscribe(int32) override { return Result<void>::Success(); }
    Result<void> UnsubscribeAll(EventType) override { return Result<void>::Success(); }
    Result<std::vector<DomainEvent*>> GetEventHistory(EventType, int32 = 100) const override {
        return Result<std::vector<DomainEvent*>>::Success({});
    }
    Result<void> ClearEventHistory() override {
        events.clear();
        return Result<void>::Success();
    }

    mutable std::vector<DomainEvent> events;
};

class FlakyMotionStatePort final : public IMotionStatePort {
public:
    Result<Point2D> GetCurrentPosition() const override { return Result<Point2D>::Success({0.0f, 0.0f}); }
    Result<float32> GetAxisPosition(LogicalAxisId) const override { return Result<float32>::Success(0.0f); }
    Result<float32> GetAxisVelocity(LogicalAxisId) const override { return Result<float32>::Success(0.0f); }
    Result<MotionStatus> GetAxisStatus(LogicalAxisId) const override { return Result<MotionStatus>::Success(status); }
    Result<bool> IsAxisMoving(LogicalAxisId) const override { return Result<bool>::Success(false); }
    Result<bool> IsAxisInPosition(LogicalAxisId) const override { return Result<bool>::Success(true); }
    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        const auto call = calls.fetch_add(1);
        if (call == 0) {
            return Result<std::vector<MotionStatus>>::Failure(Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "motion state temporarily unavailable",
                "FlakyMotionStatePort"));
        }
        return Result<std::vector<MotionStatus>>::Success({status});
    }

    mutable std::atomic<int> calls{0};
    MotionStatus status{};
};

class StableIOPort final : public IIOControlPort {
public:
    Result<IOStatus> ReadDigitalInput(int16 channel) override {
        IOStatus status;
        status.channel = channel;
        status.signal_active = false;
        status.value = 0;
        return Result<IOStatus>::Success(status);
    }
    Result<IOStatus> ReadDigitalOutput(int16 channel) override { return ReadDigitalInput(channel); }
    Result<void> WriteDigitalOutput(int16, bool) override { return Result<void>::Success(); }
    Result<bool> ReadLimitStatus(LogicalAxisId, bool) override { return Result<bool>::Success(false); }
    Result<bool> ReadServoAlarm(LogicalAxisId) override { return Result<bool>::Success(false); }
};

class FakeHomingPort final : public IHomingPort {
public:
    Result<void> HomeAxis(LogicalAxisId) override { return Result<void>::Success(); }
    Result<void> StopHoming(LogicalAxisId) override { return Result<void>::Success(); }
    Result<void> ResetHomingState(LogicalAxisId) override { return Result<void>::Success(); }
    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        status.state = HomingState::HOMED;
        return Result<HomingStatus>::Success(status);
    }
    Result<bool> IsAxisHomed(LogicalAxisId) const override { return Result<bool>::Success(true); }
    Result<bool> IsHomingInProgress(LogicalAxisId) const override { return Result<bool>::Success(false); }
    Result<void> WaitForHomingComplete(LogicalAxisId, int32 = 30000) override {
        return Result<void>::Success();
    }
};

bool WaitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

TEST(MotionOwnerBehaviorTest, MonitoringPublishesFailureAndRecoveryEvidence) {
    auto motion_state_port = std::make_shared<FlakyMotionStatePort>();
    auto io_port = std::make_shared<StableIOPort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto diagnostics_port = std::make_shared<FakeDiagnosticsPort>();
    auto event_port = std::make_shared<FakeEventPublisher>();

    motion_state_port->status.state = MotionState::IDLE;
    MotionMonitoringUseCase use_case(
        motion_state_port,
        io_port,
        homing_port,
        nullptr,
        diagnostics_port,
        event_port);

    ASSERT_TRUE(use_case.StartStatusUpdate(10).IsSuccess());
    ASSERT_TRUE(WaitUntil([&]() { return event_port->events.size() >= 2; }, std::chrono::milliseconds(500)));
    use_case.StopStatusUpdate();

    ASSERT_GE(diagnostics_port->diagnostics.size(), 2U);
    EXPECT_EQ(diagnostics_port->diagnostics.front().component, "motion_status_poll");
    EXPECT_NE(diagnostics_port->diagnostics.front().error_code, 0);
    EXPECT_EQ(diagnostics_port->diagnostics.back().component, "motion_status_poll");
    EXPECT_EQ(diagnostics_port->diagnostics.back().error_code, 0);

    ASSERT_GE(event_port->events.size(), 2U);
    EXPECT_EQ(event_port->events.front().type, EventType::WORKFLOW_STAGE_FAILED);
    EXPECT_EQ(event_port->events.back().type, EventType::WORKFLOW_STAGE_CHANGED);
}

TEST(MotionOwnerBehaviorTest, RuntimeAssemblyFactoryRejectsMissingDependencies) {
    auto result = MotionRuntimeAssemblyFactory::Create({});

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

}  // namespace
