#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"

#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using DispensingExecutionUseCase = Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using EmergencyStopUseCase = Siligen::Application::UseCases::System::EmergencyStopUseCase;
using JobID = Siligen::Application::UseCases::Dispensing::JobID;
using MotionControlUseCase = Siligen::Application::UseCases::Motion::MotionControlUseCase;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using Point2D = Siligen::Shared::Types::Point2D;
using Result = Siligen::Shared::Types::Result<Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs>;
using RuntimeExecutionSupervisionBackend =
    Siligen::Runtime::Service::Supervision::RuntimeExecutionSupervisionBackend;
using RuntimeJobStatusResponse = Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;

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

    Siligen::Shared::Types::Result<MotionStatus> GetAxisMotionStatus(
        Siligen::Shared::Types::LogicalAxisId axis) const override {
        const auto index = axis == Siligen::Shared::Types::LogicalAxisId::Y ? 1U : 0U;
        if (index < next_statuses.size()) {
            return Siligen::Shared::Types::Result<MotionStatus>::Success(next_statuses[index]);
        }
        return Siligen::Shared::Types::Result<MotionStatus>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::NOT_FOUND, "status missing"));
    }

    Siligen::Shared::Types::Result<std::vector<MotionStatus>> GetAllAxesMotionStatus() const override {
        return Siligen::Shared::Types::Result<std::vector<MotionStatus>>::Success(next_statuses);
    }

    Siligen::Shared::Types::Result<Point2D> GetCurrentPosition() const override {
        return Siligen::Shared::Types::Result<Point2D>::Success(Point2D{0.0f, 0.0f});
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
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }

    Siligen::Shared::Types::Result<bool> ReadServoAlarmStatus(Siligen::Shared::Types::LogicalAxisId) const override {
        return Siligen::Shared::Types::Result<bool>::Success(false);
    }
};

class FakeInterlockSignalPort final : public Siligen::Domain::Safety::Ports::IInterlockSignalPort {
   public:
    Siligen::Domain::Safety::ValueObjects::InterlockSignals signals{};
    bool latched = false;

    Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockSignals> ReadSignals()
        const noexcept override {
        return Siligen::Shared::Types::Result<Siligen::Domain::Safety::ValueObjects::InterlockSignals>::Success(
            signals);
    }

    bool IsLatched() const noexcept override { return latched; }
};

std::shared_ptr<MotionControlUseCase> BuildMotionControlUseCase() {
    auto monitoring = std::make_shared<FakeMotionMonitoringOperations>();
    MotionStatus x_status;
    x_status.state = Siligen::Domain::Motion::Ports::MotionState::IDLE;
    x_status.home_signal = false;
    MotionStatus y_status;
    y_status.state = Siligen::Domain::Motion::Ports::MotionState::IDLE;
    y_status.home_signal = false;
    monitoring->next_statuses = {x_status, y_status};

    return std::make_shared<MotionControlUseCase>(
        std::make_shared<FakeMotionHomingOperations>(),
        std::make_shared<FakeMotionManualOperations>(),
        monitoring);
}

std::shared_ptr<DispensingExecutionUseCase> BuildExecutionUseCase() {
    return std::make_shared<DispensingExecutionUseCase>(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
}

RuntimeJobStatusResponse MakeJobStatus(const std::string& job_id, const std::string& state) {
    RuntimeJobStatusResponse status;
    status.job_id = job_id;
    status.plan_id = "plan-1";
    status.plan_fingerprint = "fp-1";
    status.state = state;
    status.target_count = 1;
    status.completed_count = state == "completed" ? 1U : 0U;
    status.current_cycle = 1;
    status.total_segments = 10;
    status.cycle_progress_percent = state == "completed" ? 100U : 50U;
    status.overall_progress_percent = status.cycle_progress_percent;
    return status;
}

TEST(RuntimeExecutionSupervisionBackendTest, ReadsActiveJobStateDirectlyFromRuntimeExecutionUseCase) {
    auto execution_use_case = BuildExecutionUseCase();
    execution_use_case->SeedJobStateForTesting(MakeJobStatus("job-1", "paused"));
    execution_use_case->SetActiveJobForTesting("job-1");

    auto backend = RuntimeExecutionSupervisionBackend(
        BuildMotionControlUseCase(),
        std::shared_ptr<EmergencyStopUseCase>(),
        execution_use_case,
        nullptr,
        nullptr);

    auto result = backend.ReadInputs();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().active_job_id, "job-1");
    EXPECT_TRUE(result.Value().active_job_status_available);
    EXPECT_EQ(result.Value().active_job_state, "paused");
}

TEST(RuntimeExecutionSupervisionBackendTest, TerminalRuntimeJobStateIsNotExposedAsActiveJob) {
    auto execution_use_case = BuildExecutionUseCase();
    execution_use_case->SeedJobStateForTesting(MakeJobStatus("job-terminal", "completed"));
    execution_use_case->SetActiveJobForTesting("job-terminal");

    auto backend = RuntimeExecutionSupervisionBackend(
        BuildMotionControlUseCase(),
        std::shared_ptr<EmergencyStopUseCase>(),
        execution_use_case,
        nullptr,
        nullptr);

    auto result = backend.ReadInputs();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().active_job_id.empty());
    EXPECT_FALSE(result.Value().active_job_status_available);
    EXPECT_TRUE(result.Value().active_job_state.empty());
}

TEST(RuntimeExecutionSupervisionBackendTest, MissingRuntimeJobStatusDropsStaleActiveJobId) {
    auto execution_use_case = BuildExecutionUseCase();
    execution_use_case->SetActiveJobForTesting("job-missing");

    auto backend = RuntimeExecutionSupervisionBackend(
        BuildMotionControlUseCase(),
        std::shared_ptr<EmergencyStopUseCase>(),
        execution_use_case,
        nullptr,
        nullptr);

    auto result = backend.ReadInputs();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().active_job_id.empty());
    EXPECT_FALSE(result.Value().active_job_status_available);
    EXPECT_TRUE(result.Value().active_job_state.empty());
}

TEST(RuntimeExecutionSupervisionBackendTest, ReadsInterlockSignalsDirectlyFromInterlockPort) {
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    interlock_port->signals.emergency_stop_triggered = true;
    interlock_port->signals.safety_door_open = true;
    interlock_port->latched = true;

    auto backend = RuntimeExecutionSupervisionBackend(
        BuildMotionControlUseCase(),
        std::shared_ptr<EmergencyStopUseCase>(),
        BuildExecutionUseCase(),
        interlock_port,
        nullptr);

    auto result = backend.ReadInputs();

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().io.estop);
    EXPECT_TRUE(result.Value().io.estop_known);
    EXPECT_TRUE(result.Value().io.door);
    EXPECT_TRUE(result.Value().io.door_known);
    EXPECT_TRUE(result.Value().interlock_latched);
}

}  // namespace
