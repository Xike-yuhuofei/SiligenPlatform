#include <gtest/gtest.h>

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef SILIGEN_TEST_HOOKS
#define SILIGEN_TEST_HOOKS
#endif
#define private public
#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#undef private
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRequest;
using Siligen::Application::UseCases::Dispensing::PreparePlanRuntimeOverrides;
using Siligen::Application::UseCases::Dispensing::IUploadFilePort;
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Domain::Safety::ValueObjects::InterlockSignals;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Workflow::Contracts::IWorkflowExecutionPort;
using Siligen::Workflow::Contracts::WorkflowExecutionStartRequest;
using Siligen::Workflow::Contracts::WorkflowExecutionStatus;
using Siligen::Workflow::Contracts::WorkflowJobId;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

class LinePathSourceStub final : public Siligen::Domain::Trajectory::Ports::IPathSourcePort {
   public:
    Result<Siligen::Domain::Trajectory::Ports::PathSourceResult> LoadFromFile(const std::string&) override {
        using Siligen::Domain::Trajectory::Ports::PathPrimitiveMeta;
        using Siligen::Domain::Trajectory::Ports::PathSourceResult;
        using Siligen::Domain::Trajectory::ValueObjects::Primitive;

        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(20.0f, 0.0f)));
        result.metadata.push_back(PathPrimitiveMeta{});
        return Result<PathSourceResult>::Success(result);
    }
};

class ScopedTempPbFile {
   public:
    ScopedTempPbFile() {
        path_ = std::filesystem::temp_directory_path() /
                ("siligen-dispensing-" + std::to_string(std::rand()) + ".pb");
        std::ofstream output(path_, std::ios::binary);
        output << "pb";
    }

    ~ScopedTempPbFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    const std::string string() const { return path_.string(); }

   private:
    std::filesystem::path path_;
};

PlanningRequest BuildCanonicalPlanningRequest(const std::string& filepath = "canonical.dxf") {
    PlanningRequest request;
    request.dxf_filepath = filepath;
    request.optimize_path = true;
    request.start_x = 12.5f;
    request.start_y = -3.0f;
    request.approximate_splines = true;
    request.two_opt_iterations = 7;
    request.spline_max_step_mm = 0.25f;
    request.spline_max_error_mm = 0.05f;
    request.continuity_tolerance_mm = 0.02f;
    request.curve_chain_angle_deg = 15.0f;
    request.curve_chain_max_segment_mm = 1.5f;
    request.use_hardware_trigger = false;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = Siligen::Domain::Motion::InterpolationAlgorithm::SPLINE;
    request.trajectory_config.max_velocity = 88.0f;
    request.trajectory_config.max_acceleration = 350.0f;
    request.trajectory_config.max_jerk = 500.0f;
    request.trajectory_config.arc_tolerance = 0.1f;
    return request;
}

PreparePlanRuntimeOverrides BuildPreparePlanRuntimeOverrides() {
    PreparePlanRuntimeOverrides overrides;
    overrides.use_hardware_trigger = false;
    overrides.dry_run = true;
    overrides.max_jerk = 500.0f;
    overrides.arc_tolerance_mm = 0.1f;
    overrides.dispensing_speed_mm_s = 22.0f;
    overrides.dry_run_speed_mm_s = 88.0f;
    overrides.rapid_speed_mm_s = 120.0f;
    overrides.acceleration_mm_s2 = 350.0f;
    overrides.velocity_trace_enabled = true;
    overrides.velocity_trace_interval_ms = 25;
    overrides.velocity_trace_path = "logs/trace.csv";
    overrides.velocity_guard_enabled = true;
    overrides.velocity_guard_ratio = 0.2f;
    overrides.velocity_guard_abs_mm_s = 4.0f;
    overrides.velocity_guard_min_expected_mm_s = 6.0f;
    overrides.velocity_guard_grace_ms = 300;
    overrides.velocity_guard_interval_ms = 150;
    overrides.velocity_guard_max_consecutive = 4;
    overrides.velocity_guard_stop_on_violation = true;
    return overrides;
}

DispensingPlan BuildMinimalPlan() {
    DispensingPlan plan;
    plan.success = true;
    plan.total_length_mm = 20.0f;
    plan.estimated_time_s = 1.0f;
    plan.trigger_interval_mm = 5.0f;
    plan.motion_trajectory.total_length = 20.0f;
    plan.motion_trajectory.total_time = 1.0f;
    plan.interpolation_points.emplace_back(0.0f, 0.0f, 10.0f);
    plan.interpolation_points.emplace_back(20.0f, 0.0f, 10.0f);
    plan.interpolation_points.front().enable_position_trigger = true;
    plan.interpolation_points.back().enable_position_trigger = true;
    plan.trigger_distances_mm = {0.0f, 20.0f};
    plan.preview_authority_ready = true;
    plan.preview_authority_shared_with_execution = true;
    plan.preview_spacing_valid = true;
    return plan;
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildMinimalExecutionPackage() {
    const auto plan = BuildMinimalPlan();

    Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt built;
    built.execution_plan.interpolation_points = plan.interpolation_points;
    built.execution_plan.motion_trajectory = plan.motion_trajectory;
    built.execution_plan.trigger_distances_mm = plan.trigger_distances_mm;
    built.execution_plan.trigger_interval_ms = plan.trigger_interval_ms;
    built.execution_plan.trigger_interval_mm = plan.trigger_interval_mm;
    built.execution_plan.total_length_mm = plan.total_length_mm;
    built.total_length_mm = plan.total_length_mm;
    built.estimated_time_s = plan.estimated_time_s;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "artifact-fingerprint";
    return Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated(built);
}

std::shared_ptr<PlanningUseCase> CreateRealPlanningUseCase() {
    auto path_source = std::make_shared<LinePathSourceStub>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>(),
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(),
        std::make_shared<Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService>(),
        std::make_shared<Siligen::Application::Services::Dispensing::ExecutionAssemblyService>(),
        nullptr,
        pb_service);
}

std::shared_ptr<PlanningUseCase> CreatePlanningUseCaseWithPathSourceAndExport(
    const std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort>& path_source,
    const std::shared_ptr<IPlanningArtifactExportPort>& export_port) {
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>(),
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(),
        std::make_shared<Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService>(),
        std::make_shared<Siligen::Application::Services::Dispensing::ExecutionAssemblyService>(),
        nullptr,
        pb_service,
        export_port);
}

std::shared_ptr<PlanningUseCase> CreatePlanningUseCaseWithPathSource(
    const std::shared_ptr<Siligen::Domain::Trajectory::Ports::IPathSourcePort>& path_source) {
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>(),
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(),
        std::make_shared<Siligen::Application::Services::Dispensing::AuthorityPreviewAssemblyService>(),
        std::make_shared<Siligen::Application::Services::Dispensing::ExecutionAssemblyService>(),
        nullptr,
        pb_service);
}
class FakeHardwareConnectionPort final : public DeviceConnectionPort {
   public:
    Result<void> Connect(const DeviceConnection&) override { return Result<void>::Success(); }
    Result<void> Disconnect() override { return Result<void>::Success(); }
    Result<DeviceConnectionSnapshot> ReadConnection() const override {
        DeviceConnectionSnapshot info;
        info.state = connected ? DeviceConnectionState::Connected : DeviceConnectionState::Disconnected;
        return Result<DeviceConnectionSnapshot>::Success(info);
    }
    bool IsConnected() const override { return connected; }
    Result<void> Reconnect() override {
        connected = true;
        return Result<void>::Success();
    }
    void SetConnectionStateCallback(std::function<void(const DeviceConnectionSnapshot&)>) override {}
    Result<void> StartStatusMonitoring(uint32 = 1000) override { return Result<void>::Success(); }
    void StopStatusMonitoring() override {}
    std::string GetLastError() const override { return std::string(); }
    void ClearError() override {}
    Result<void> StartHeartbeat(const HeartbeatSnapshot&) override { return Result<void>::Success(); }
    void StopHeartbeat() override {}
    HeartbeatSnapshot ReadHeartbeat() const override { return HeartbeatSnapshot{}; }
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

class FakeWorkflowExecutionPort final : public IWorkflowExecutionPort {
   public:
    Result<WorkflowJobId> StartWorkflowExecution(const WorkflowExecutionStartRequest& request) override {
        ++start_calls;
        last_start_request = request;
        if (start_error.has_value()) {
            return Result<WorkflowJobId>::Failure(start_error.value());
        }

        WorkflowExecutionStatus status;
        status.job_id = next_job_id;
        status.plan_id = request.plan_id;
        status.plan_fingerprint = request.plan_fingerprint;
        status.state = start_state;
        status.target_count = request.target_count;
        status.dry_run = request.launch.runtime_overrides.dry_run;
        statuses[status.job_id] = status;
        return Result<WorkflowJobId>::Success(status.job_id);
    }

    Result<WorkflowExecutionStatus> GetWorkflowExecutionStatus(const WorkflowJobId& job_id) const override {
        ++status_calls;
        if (status_error.has_value()) {
            return Result<WorkflowExecutionStatus>::Failure(status_error.value());
        }

        auto it = statuses.find(job_id);
        if (it == statuses.end()) {
            return Result<WorkflowExecutionStatus>::Failure(
                Error(ErrorCode::NOT_FOUND, "job not found", "FakeWorkflowExecutionPort"));
        }
        return Result<WorkflowExecutionStatus>::Success(it->second);
    }

    Result<void> PauseWorkflowExecution(const WorkflowJobId& job_id) override {
        ++pause_calls;
        last_paused_job_id = job_id;
        if (pause_error.has_value()) {
            return Result<void>::Failure(pause_error.value());
        }
        return Result<void>::Success();
    }

    Result<void> ResumeWorkflowExecution(const WorkflowJobId& job_id) override {
        ++resume_calls;
        last_resumed_job_id = job_id;
        if (resume_error.has_value()) {
            return Result<void>::Failure(resume_error.value());
        }
        return Result<void>::Success();
    }

    Result<void> StopWorkflowExecution(const WorkflowJobId& job_id) override {
        ++stop_calls;
        last_stopped_job_id = job_id;
        if (stop_error.has_value()) {
            return Result<void>::Failure(stop_error.value());
        }
        return Result<void>::Success();
    }

    void SeedStatusForTesting(const WorkflowExecutionStatus& status) {
        statuses[status.job_id] = status;
    }

    WorkflowJobId next_job_id = "job-1";
    std::string start_state = "running";
    std::optional<Error> start_error;
    std::optional<Error> status_error;
    std::optional<Error> pause_error;
    std::optional<Error> resume_error;
    std::optional<Error> stop_error;
    mutable int status_calls = 0;
    int start_calls = 0;
    int pause_calls = 0;
    int resume_calls = 0;
    int stop_calls = 0;
    WorkflowExecutionStartRequest last_start_request{};
    WorkflowJobId last_paused_job_id;
    WorkflowJobId last_resumed_job_id;
    WorkflowJobId last_stopped_job_id;
    mutable std::unordered_map<WorkflowJobId, WorkflowExecutionStatus> statuses;
};

MotionStatus ReadyAxisStatus() {
    MotionStatus status;
    status.state = MotionState::HOMED;
    status.enabled = true;
    status.home_signal = true;
    return status;
}

void SeedAuthorityMetadata(
    DispensingWorkflowUseCase::PlanRecord& plan_record,
    const std::string& layout_id) {
    plan_record.preview_binding_ready = true;
    plan_record.preview_validation_classification = "pass";
    plan_record.authority_trigger_layout.layout_id = layout_id;
    plan_record.authority_trigger_layout.plan_id = plan_record.response.plan_id;
    plan_record.authority_trigger_layout.plan_fingerprint = plan_record.response.plan_fingerprint;
    plan_record.authority_trigger_layout.authority_ready = true;
    plan_record.authority_trigger_layout.binding_ready = true;
}

void SeedPlan(DispensingWorkflowUseCase& use_case, const std::string& plan_id) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    plan_record.execution_launch.execution_package = BuildMinimalExecutionPackage();
    plan_record.execution_launch.runtime_overrides.source_path = "artifact.pb";
    plan_record.execution_launch.runtime_overrides.use_hardware_trigger = false;
    plan_record.execution_launch.runtime_overrides.dry_run = true;
    plan_record.execution_launch.runtime_overrides.dispensing_speed_mm_s = 22.0f;
    plan_record.execution_launch.runtime_overrides.dry_run_speed_mm_s = 88.0f;
    plan_record.execution_trajectory_points.emplace_back(0.0f, 0.0f, 10.0f);
    plan_record.execution_trajectory_points.emplace_back(20.0f, 0.0f, 10.0f);
    plan_record.glue_points.emplace_back(0.0f, 0.0f);
    plan_record.glue_points.emplace_back(20.0f, 0.0f);
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    SeedAuthorityMetadata(plan_record, "layout-" + plan_id);
    use_case.plans_[plan_id] = plan_record;
}

DispensingWorkflowUseCase CreateUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<FakeWorkflowExecutionPort> execution_port = nullptr) {
    if (!execution_port) {
        execution_port = std::make_shared<FakeWorkflowExecutionPort>();
    }
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        MakeDummyShared<PlanningUseCase>(),
        execution_port,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase CreateUseCaseWithPlanning(
    const std::shared_ptr<PlanningUseCase>& planning_use_case,
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port) {
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        std::make_shared<FakeWorkflowExecutionPort>(),
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

std::shared_ptr<FakeWorkflowExecutionPort> CreateWorkflowExecutionPort() {
    return std::make_shared<FakeWorkflowExecutionPort>();
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
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    plan_record.preview_snapshot_id = plan_id;
    SeedAuthorityMetadata(plan_record, "layout-" + plan_id);
    for (const auto& point : points) {
        plan_record.execution_trajectory_points.emplace_back(point.x, point.y, 0.0f);
        plan_record.glue_points.push_back(point);
    }
    return plan_record;
}

bool SnapshotContainsPoint(
    const Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.execution_polyline) {
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
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    interlock_port->signals.safety_door_open = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
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

TEST(DispensingWorkflowUseCaseTest, PreparePlanUsesCanonicalPlanningInputAndRuntimeOverrides) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCaseWithPlanning(
        planning_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-prepare";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest request;
    request.artifact_id = artifact_record.response.artifact_id;
    request.planning_request = BuildCanonicalPlanningRequest();
    request.planning_request.dxf_filepath.clear();
    request.runtime_overrides = BuildPreparePlanRuntimeOverrides();

    auto result = use_case.PreparePlan(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& prepared = result.Value();
    auto it = use_case.plans_.find(prepared.plan_id);
    ASSERT_NE(it, use_case.plans_.end());

    const auto& stored_launch = it->second.execution_launch;
    EXPECT_EQ(stored_launch.runtime_overrides.source_path, temp_pb_file.string());
    EXPECT_TRUE(stored_launch.runtime_overrides.dry_run);
    EXPECT_FALSE(stored_launch.runtime_overrides.use_hardware_trigger);
    ASSERT_TRUE(stored_launch.runtime_overrides.dispensing_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(stored_launch.runtime_overrides.dispensing_speed_mm_s.value(), 22.0f);
    ASSERT_TRUE(stored_launch.runtime_overrides.dry_run_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(stored_launch.runtime_overrides.dry_run_speed_mm_s.value(), 88.0f);
    EXPECT_TRUE(stored_launch.runtime_overrides.velocity_trace_enabled);
    EXPECT_EQ(stored_launch.runtime_overrides.velocity_trace_interval_ms, 25);
    EXPECT_EQ(stored_launch.runtime_overrides.velocity_trace_path, "logs/trace.csv");
    EXPECT_TRUE(stored_launch.runtime_overrides.velocity_guard_stop_on_violation);
    EXPECT_GE(stored_launch.execution_package.execution_plan.motion_trajectory.points.size() +
                  stored_launch.execution_package.execution_plan.interpolation_points.size(),
              2U);
    EXPECT_EQ(prepared.filepath, temp_pb_file.string());
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsUnhomedAxis) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    auto y_status = ReadyAxisStatus();
    y_status.state = MotionState::IDLE;
    y_status.home_signal = true;
    motion_state_port->statuses[LogicalAxisId::Y] = y_status;
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = false;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
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
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    interlock_port->signals.emergency_stop_triggered = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-1");

    Siligen::Application::UseCases::Dispensing::StartJobRequest start_request;
    start_request.plan_id = "plan-1";
    start_request.plan_fingerprint = "fp-plan-1";
    start_request.target_count = 1;

    auto start_result = use_case.StartJob(start_request);
    ASSERT_TRUE(start_result.IsError());
    EXPECT_EQ(start_result.GetError().GetCode(), ErrorCode::EMERGENCY_STOP_ACTIVATED);

    WorkflowExecutionStatus runtime_status;
    runtime_status.job_id = "job-1";
    runtime_status.plan_id = "plan-1";
    runtime_status.plan_fingerprint = "fp-plan-1";
    runtime_status.state = "paused";
    runtime_status.target_count = 1;
    execution_use_case->SeedStatusForTesting(runtime_status);

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
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    SeedAuthorityMetadata(plan_record, "layout-plan-confirmed");
    plan_record.execution_trajectory_points.emplace_back(0.0f, 0.0f, 0.0f);
    plan_record.execution_trajectory_points.emplace_back(10.0f, 0.0f, 0.0f);
    plan_record.glue_points.emplace_back(0.0f, 0.0f);
    plan_record.glue_points.emplace_back(10.0f, 0.0f);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-confirmed";
    request.max_polyline_points = 16;
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_EQ(snapshot.preview_state, "confirmed");
    EXPECT_EQ(snapshot.preview_source, "planned_glue_snapshot");
    EXPECT_EQ(snapshot.preview_kind, "glue_points");
    EXPECT_EQ(snapshot.confirmed_at, "2026-03-22T00:00:00Z");
    EXPECT_EQ(snapshot.snapshot_hash, "fp-plan-confirmed");
    EXPECT_EQ(snapshot.plan_id, "plan-confirmed");
    EXPECT_EQ(snapshot.glue_point_count, 2U);
    EXPECT_EQ(snapshot.glue_points.size(), 2U);
}

TEST(DispensingWorkflowUseCaseTest, ConfirmPreviewRejectsMissingAuthorityLayoutId) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-missing-layout",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    plan_record.authority_trigger_layout.layout_id.clear();
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest request;
    request.plan_id = "plan-missing-layout";
    request.snapshot_hash = "fp-plan-missing-layout";
    auto result = use_case.ConfirmPreview(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("layout"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, ConfirmPreviewAllowsPassWithExceptionMetadata) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-exception",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    plan_record.preview_validation_classification = "pass_with_exception";
    plan_record.preview_exception_reason = "short closed span kept as explicit exception";
    plan_record.preview_spacing_valid = true;
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest request;
    request.plan_id = "plan-exception";
    request.snapshot_hash = "fp-plan-exception";
    const auto result = use_case.ConfirmPreview(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().preview_state, "confirmed");
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsFailPreviewUsingFailureReason) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-authority-fail");
    auto& plan_record = use_case.plans_.at("plan-authority-fail");
    plan_record.preview_validation_classification = "fail";
    plan_record.preview_spacing_valid = false;
    plan_record.preview_failure_reason = "authority locator failed on degenerate spline";

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-authority-fail";
    request.plan_fingerprint = "fp-plan-authority-fail";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "authority locator failed on degenerate spline");
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
    EXPECT_FALSE(SnapshotContainsPoint(snapshot, 10.0f, 10.0f, 1e-4f));
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
    EXPECT_LE(snapshot.execution_polyline.size(), 6U);
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
    ASSERT_GE(snapshot.execution_polyline.size(), 2U);
    EXPECT_NEAR(snapshot.execution_polyline.front().x, 0.0f, 1e-4f);
    EXPECT_NEAR(snapshot.execution_polyline.back().x, 30.0f, 1e-4f);
    EXPECT_EQ(snapshot.execution_polyline.size(), 11U);
    for (std::size_t i = 1; i < snapshot.execution_polyline.size(); ++i) {
        const auto& prev = snapshot.execution_polyline[i - 1U];
        const auto& curr = snapshot.execution_polyline[i];
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        const double distance = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(distance, 3.0, 1e-2) << "spacing at segment index " << i;
    }
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotFailsWhenPreviewAuthorityIsUnavailable) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = "plan-missing-authority";
    plan_record.response.plan_fingerprint = "fp-plan-missing-authority";
    plan_record.preview_authority_ready = false;
    plan_record.preview_authority_shared_with_execution = false;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_failure_reason = "preview authority unavailable";
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::PREPARED;
    plan_record.latest = true;
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-missing-authority";
    auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(use_case.plans_.at("plan-missing-authority").preview_state,
              DispensingWorkflowUseCase::PlanPreviewState::FAILED);
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotFailsAtSnapshotStageWhenBindingUnavailable) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    SeedPlan(use_case, "plan-binding-unavailable");
    auto& plan_record = use_case.plans_.at("plan-binding-unavailable");
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::PREPARED;
    plan_record.preview_binding_ready = false;
    plan_record.preview_failure_reason = "authority trigger binding unavailable";

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-binding-unavailable";
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "authority trigger binding unavailable");
    EXPECT_EQ(
        use_case.plans_.at("plan-binding-unavailable").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::FAILED);
    EXPECT_EQ(
        use_case.plans_.at("plan-binding-unavailable").failure_message,
        "authority trigger binding unavailable");
}

TEST(DispensingWorkflowUseCaseTest, PreviewGateFailureReasonIsConsistentAcrossSnapshotConfirmAndStart) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    auto snapshot_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto confirm_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto start_case =
        CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);

    const std::string expected_reason = "authority trigger binding unavailable";

    auto snapshot_plan = BuildPreviewPlanRecord(
        "plan-gate-snapshot",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    snapshot_plan.preview_state = DispensingWorkflowUseCase::PlanPreviewState::PREPARED;
    snapshot_plan.preview_binding_ready = false;
    snapshot_plan.preview_failure_reason = expected_reason;
    snapshot_case.plans_[snapshot_plan.response.plan_id] = snapshot_plan;

    auto confirm_plan = BuildPreviewPlanRecord(
        "plan-gate-confirm",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    confirm_plan.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    confirm_plan.preview_binding_ready = false;
    confirm_plan.preview_failure_reason = expected_reason;
    confirm_case.plans_[confirm_plan.response.plan_id] = confirm_plan;

    SeedPlan(start_case, "plan-gate-start");
    auto& start_plan = start_case.plans_.at("plan-gate-start");
    start_plan.preview_binding_ready = false;
    start_plan.preview_failure_reason = expected_reason;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = "plan-gate-snapshot";
    const auto snapshot_result = snapshot_case.GetPreviewSnapshot(snapshot_request);

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest confirm_request;
    confirm_request.plan_id = "plan-gate-confirm";
    confirm_request.snapshot_hash = "fp-plan-gate-confirm";
    const auto confirm_result = confirm_case.ConfirmPreview(confirm_request);

    Siligen::Application::UseCases::Dispensing::StartJobRequest start_request;
    start_request.plan_id = "plan-gate-start";
    start_request.plan_fingerprint = "fp-plan-gate-start";
    start_request.target_count = 1;
    const auto start_result = start_case.StartJob(start_request);

    ASSERT_TRUE(snapshot_result.IsError());
    ASSERT_TRUE(confirm_result.IsError());
    ASSERT_TRUE(start_result.IsError());
    EXPECT_EQ(snapshot_result.GetError().GetMessage(), expected_reason);
    EXPECT_EQ(confirm_result.GetError().GetMessage(), expected_reason);
    EXPECT_EQ(start_result.GetError().GetMessage(), expected_reason);
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsPreviewAuthorityMismatchBeforeRuntimeLaunch) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-authority-mismatch");
    auto& plan_record = use_case.plans_.at("plan-authority-mismatch");
    plan_record.preview_authority_shared_with_execution = false;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-authority-mismatch";
    request.plan_fingerprint = "fp-plan-authority-mismatch";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("not shared"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsPreviewBindingUnavailableBeforeRuntimeLaunch) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-binding-mismatch");
    auto& plan_record = use_case.plans_.at("plan-binding-mismatch");
    plan_record.preview_binding_ready = false;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-binding-mismatch";
    request.plan_fingerprint = "fp-plan-binding-mismatch";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("binding"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, FinalizeJobClearsConfirmedPreviewState) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case = CreateWorkflowExecutionPort();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);

    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = "plan-finish";
    plan_record.response.plan_fingerprint = "fp-plan-finish";
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = "fp-plan-finish";
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    plan_record.runtime_job_id = "job-finish";
    SeedAuthorityMetadata(plan_record, "layout-plan-finish");
    use_case.plans_[plan_record.response.plan_id] = plan_record;
    use_case.job_plan_index_["job-finish"] = "plan-finish";

    WorkflowExecutionStatus runtime_status;
    runtime_status.job_id = "job-finish";
    runtime_status.plan_id = "plan-finish";
    runtime_status.plan_fingerprint = "fp-plan-finish";
    runtime_status.state = "running";
    runtime_status.target_count = 1;
    execution_use_case->SeedStatusForTesting(runtime_status);

    auto stop_result = use_case.StopJob(runtime_status.job_id);
    ASSERT_TRUE(stop_result.IsSuccess());

    ASSERT_TRUE(use_case.plans_.find("plan-finish") != use_case.plans_.end());
    EXPECT_EQ(
        use_case.plans_.at("plan-finish").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY);
    EXPECT_TRUE(use_case.plans_.at("plan-finish").confirmed_at.empty());
}
