#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define private public
#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#undef private

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::Domain::Machine::Ports::HardwareConnectionConfig;
using Siligen::Domain::Machine::Ports::HardwareConnectionInfo;
using Siligen::Domain::Machine::Ports::HardwareConnectionState;
using Siligen::Domain::Machine::Ports::HeartbeatConfig;
using Siligen::Domain::Machine::Ports::HeartbeatStatus;
using Siligen::Domain::Machine::Ports::IHardwareConnectionPort;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Domain::Safety::ValueObjects::InterlockSignals;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

class FakeHardwareConnectionPort final : public IHardwareConnectionPort {
   public:
    Result<void> Connect(const HardwareConnectionConfig&) override { return Result<void>::Success(); }
    Result<void> Disconnect() override { return Result<void>::Success(); }
    HardwareConnectionInfo GetConnectionInfo() const override {
        HardwareConnectionInfo info;
        info.state = connected ? HardwareConnectionState::Connected : HardwareConnectionState::Disconnected;
        return info;
    }
    bool IsConnected() const override { return connected; }
    Result<void> Reconnect() override {
        connected = true;
        return Result<void>::Success();
    }
    void SetConnectionStateCallback(std::function<void(const HardwareConnectionInfo&)>) override {}
    Result<void> StartStatusMonitoring(uint32 = 1000) override { return Result<void>::Success(); }
    void StopStatusMonitoring() override {}
    std::string GetLastError() const override { return std::string(); }
    void ClearError() override {}
    Result<void> StartHeartbeat(const HeartbeatConfig&) override { return Result<void>::Success(); }
    void StopHeartbeat() override {}
    HeartbeatStatus GetHeartbeatStatus() const override { return HeartbeatStatus{}; }
    Result<bool> Ping() const override { return Result<bool>::Success(connected); }

    bool connected = true;
};

class FakeMotionStatePort final : public IMotionStatePort {
   public:
    Result<Point2D> GetCurrentPosition() const override { return Result<Point2D>::Success(Point2D{}); }
    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        return Result<float32>::Success(statuses.at(axis).position.x);
    }
    Result<float32> GetAxisVelocity(LogicalAxisId axis) const override {
        return Result<float32>::Success(statuses.at(axis).velocity);
    }
    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        auto it = statuses.find(axis);
        if (it == statuses.end()) {
            return Result<MotionStatus>::Success(MotionStatus{});
        }
        return Result<MotionStatus>::Success(it->second);
    }
    Result<bool> IsAxisMoving(LogicalAxisId axis) const override {
        return Result<bool>::Success(statuses.at(axis).state == MotionState::MOVING);
    }
    Result<bool> IsAxisInPosition(LogicalAxisId) const override { return Result<bool>::Success(true); }
    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        std::vector<MotionStatus> all;
        for (const auto& entry : statuses) {
            all.push_back(entry.second);
        }
        return Result<std::vector<MotionStatus>>::Success(all);
    }

    std::unordered_map<LogicalAxisId, MotionStatus> statuses;
};

class FakeInterlockSignalPort final : public IInterlockSignalPort {
   public:
    Result<InterlockSignals> ReadSignals() const noexcept override {
        return Result<InterlockSignals>::Success(signals);
    }

    InterlockSignals signals{};
};

class FakeHomingPort final : public IHomingPort {
   public:
    Result<void> HomeAxis(LogicalAxisId) override { return Result<void>::Success(); }
    Result<void> StopHoming(LogicalAxisId) override { return Result<void>::Success(); }
    Result<void> ResetHomingState(LogicalAxisId axis) override {
        homed[axis] = false;
        return Result<void>::Success();
    }
    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        status.state = Lookup(axis) ? HomingState::HOMED : HomingState::NOT_HOMED;
        return Result<HomingStatus>::Success(status);
    }
    Result<bool> IsAxisHomed(LogicalAxisId axis) const override { return Result<bool>::Success(Lookup(axis)); }
    Result<bool> IsHomingInProgress(LogicalAxisId) const override { return Result<bool>::Success(false); }
    Result<void> WaitForHomingComplete(LogicalAxisId, Siligen::Shared::Types::int32) override {
        return Result<void>::Success();
    }

    std::unordered_map<LogicalAxisId, bool> homed;

   private:
    bool Lookup(LogicalAxisId axis) const {
        auto it = homed.find(axis);
        return it != homed.end() && it->second;
    }
};

MotionStatus ReadyAxisStatus() {
    MotionStatus status;
    status.state = MotionState::HOMED;
    status.enabled = true;
    status.home_signal = true;
    return status;
}

void SeedPlan(DispensingWorkflowUseCase& use_case, const std::string& plan_id) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    use_case.plans_[plan_id] = plan_record;
}

DispensingWorkflowUseCase CreateUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port) {
    return DispensingWorkflowUseCase(
        MakeDummyShared<UploadFileUseCase>(),
        MakeDummyShared<PlanningUseCase>(),
        MakeDummyShared<DispensingExecutionUseCase>(),
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase::PlanRecord BuildPreviewPlanRecord(
    const std::string& plan_id,
    const std::vector<Point2D>& points) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    plan_record.response.segment_count = static_cast<std::uint32_t>(points.size() > 1 ? points.size() - 1 : 0);
    plan_record.response.point_count = static_cast<std::uint32_t>(points.size());
    plan_record.response.total_length_mm = 0.0f;
    plan_record.response.estimated_time_s = 1.0f;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    plan_record.preview_snapshot_id = plan_id;
    for (const auto& point : points) {
        plan_record.trajectory_points.emplace_back(point.x, point.y, 0.0f);
    }
    return plan_record;
}

bool SnapshotContainsPoint(
    const Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.trajectory_polyline) {
        if (std::abs(point.x - target_x) <= tolerance && std::abs(point.y - target_y) <= tolerance) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsSafetyDoor) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    interlock_port->signals.safety_door_open = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    SeedPlan(use_case, "plan-door");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-door";
    request.plan_fingerprint = "fp-plan-door";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_FALSE(result.GetError().GetMessage().empty());
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsUnhomedAxis) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    auto y_status = ReadyAxisStatus();
    y_status.state = MotionState::IDLE;
    y_status.home_signal = true;
    motion_state_port->statuses[LogicalAxisId::Y] = y_status;
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = false;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    SeedPlan(use_case, "plan-home");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-home";
    request.plan_fingerprint = "fp-plan-home";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::AXIS_NOT_HOMED);
}

TEST(DispensingWorkflowUseCaseTest, StartAndResumeUseSameInterlockPreconditions) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    interlock_port->signals.emergency_stop_triggered = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    SeedPlan(use_case, "plan-1");

    Siligen::Application::UseCases::Dispensing::StartJobRequest start_request;
    start_request.plan_id = "plan-1";
    start_request.plan_fingerprint = "fp-plan-1";
    start_request.target_count = 1;

    auto start_result = use_case.StartJob(start_request);
    ASSERT_TRUE(start_result.IsError());
    EXPECT_EQ(start_result.GetError().GetCode(), ErrorCode::EMERGENCY_STOP_ACTIVATED);

    auto context = std::make_shared<DispensingWorkflowUseCase::JobContext>();
    context->job_id = "job-1";
    context->state.store(DispensingWorkflowUseCase::WorkflowJobState::PAUSED);
    context->pause_requested.store(true);
    use_case.jobs_["job-1"] = context;

    auto resume_result = use_case.ResumeJob("job-1");
    ASSERT_TRUE(resume_result.IsError());
    EXPECT_EQ(resume_result.GetError().GetCode(), ErrorCode::EMERGENCY_STOP_ACTIVATED);
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotKeepsConfirmedStateWhenFingerprintUnchanged) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = "plan-confirmed";
    plan_record.response.plan_fingerprint = "fp-plan-confirmed";
    plan_record.response.segment_count = 1;
    plan_record.response.point_count = 2;
    plan_record.response.total_length_mm = 10.0f;
    plan_record.response.estimated_time_s = 1.0f;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    plan_record.trajectory_points.emplace_back(0.0f, 0.0f, 0.0f);
    plan_record.trajectory_points.emplace_back(10.0f, 0.0f, 0.0f);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-confirmed";
    request.max_polyline_points = 16;
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_EQ(snapshot.preview_state, "confirmed");
    EXPECT_EQ(snapshot.confirmed_at, "2026-03-22T00:00:00Z");
    EXPECT_EQ(snapshot.snapshot_hash, "fp-plan-confirmed");
    EXPECT_EQ(snapshot.plan_id, "plan-confirmed");
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotSuppressesShortABATailArtifacts) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    std::vector<Point2D> raw_points{
        Point2D(0.0f, 0.0f),
        Point2D(10.0f, 0.0f),
        Point2D(10.0f, 10.0f),
        Point2D(10.2f, 10.0f),
        Point2D(10.0f, 10.0f),
        Point2D(0.0f, 10.0f),
    };
    auto plan_record = BuildPreviewPlanRecord("plan-tail", raw_points);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-tail";
    request.max_polyline_points = 64;
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_FALSE(SnapshotContainsPoint(snapshot, 10.2f, 10.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(snapshot, 10.0f, 10.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(snapshot, 0.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(SnapshotContainsPoint(snapshot, 0.0f, 10.0f, 1e-4f));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotKeepsCornerWhenDownsampling) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    std::vector<Point2D> raw_points;
    for (int i = 0; i <= 9; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }
    for (int j = 1; j <= 10; ++j) {
        raw_points.emplace_back(9.0f, static_cast<float>(j));
    }

    auto plan_record = BuildPreviewPlanRecord("plan-corner", raw_points);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-corner";
    request.max_polyline_points = 6;
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_LE(snapshot.trajectory_polyline.size(), 6U);
    EXPECT_TRUE(SnapshotContainsPoint(snapshot, 9.0f, 0.0f, 1e-4f));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotUsesThreeMillimeterCenterSpacing) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    std::vector<Point2D> raw_points;
    for (int i = 0; i <= 30; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }

    auto plan_record = BuildPreviewPlanRecord("plan-spacing-3mm", raw_points);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-spacing-3mm";
    request.max_polyline_points = 128;
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    ASSERT_GE(snapshot.trajectory_polyline.size(), 2U);
    EXPECT_NEAR(snapshot.trajectory_polyline.front().x, 0.0f, 1e-4f);
    EXPECT_NEAR(snapshot.trajectory_polyline.back().x, 30.0f, 1e-4f);
    EXPECT_EQ(snapshot.trajectory_polyline.size(), 11U);
    for (std::size_t i = 1; i < snapshot.trajectory_polyline.size(); ++i) {
        const auto& prev = snapshot.trajectory_polyline[i - 1U];
        const auto& curr = snapshot.trajectory_polyline[i];
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        const double distance = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(distance, 3.0, 1e-2) << "spacing at segment index " << i;
    }
}

TEST(DispensingWorkflowUseCaseTest, FinalizeJobClearsConfirmedPreviewState) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = "plan-finish";
    plan_record.response.plan_fingerprint = "fp-plan-finish";
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = "fp-plan-finish";
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    auto context = std::make_shared<DispensingWorkflowUseCase::JobContext>();
    context->job_id = "job-finish";
    context->plan_id = "plan-finish";
    context->target_count.store(1);
    use_case.jobs_[context->job_id] = context;
    use_case.active_job_id_ = "job-finish";

    auto stop_result = use_case.StopJob(context->job_id);
    ASSERT_TRUE(stop_result.IsSuccess());

    ASSERT_TRUE(use_case.plans_.find("plan-finish") != use_case.plans_.end());
    EXPECT_EQ(
        use_case.plans_.at("plan-finish").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY);
    EXPECT_TRUE(use_case.plans_.at("plan-finish").confirmed_at.empty());
}
