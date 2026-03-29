#include <gtest/gtest.h>

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef SILIGEN_TEST_HOOKS
#define SILIGEN_TEST_HOOKS
#endif
#define private public
#include "application/services/dispensing/DispensePlanningFacade.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#undef private
#include "application/services/dispensing/DispensingExecutionCompatibilityService.h"
#include "application/usecases/dispensing/DispensingExecutionWorkflowUseCase.h"
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

namespace {

using Siligen::Application::Services::Dispensing::DispensingExecutionCompatibilityService;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionRequest;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingMVPRequest;
using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::JobExecutionContext;
using Siligen::Application::UseCases::Dispensing::JobState;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRequest;
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

DispensingMVPRequest BuildLegacyRequest(const std::string& filepath = "legacy.dxf") {
    DispensingMVPRequest request;
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
    request.dry_run = true;
    request.max_jerk = 500.0f;
    request.arc_tolerance_mm = 0.1f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = Siligen::Domain::Motion::InterpolationAlgorithm::SPLINE;
    request.dispensing_speed_mm_s = 22.0f;
    request.dry_run_speed_mm_s = 88.0f;
    request.rapid_speed_mm_s = 120.0f;
    request.acceleration_mm_s2 = 350.0f;
    request.velocity_trace_enabled = true;
    request.velocity_trace_interval_ms = 25;
    request.velocity_trace_path = "logs/trace.csv";
    request.velocity_guard_enabled = true;
    request.velocity_guard_ratio = 0.2f;
    request.velocity_guard_abs_mm_s = 4.0f;
    request.velocity_guard_min_expected_mm_s = 6.0f;
    request.velocity_guard_grace_ms = 300;
    request.velocity_guard_interval_ms = 150;
    request.velocity_guard_max_consecutive = 4;
    request.velocity_guard_stop_on_violation = true;
    return request;
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

PlanningResponse BuildPlanningResponseWithExecutionPlan() {
    PlanningResponse response;
    const auto plan = BuildMinimalPlan();
    response.success = true;
    response.segment_count = 1;
    response.total_length = 20.0f;
    response.estimated_time = 1.0f;
    response.execution_trajectory_points.emplace_back(0.0f, 0.0f, 10.0f);
    response.execution_trajectory_points.emplace_back(20.0f, 0.0f, 10.0f);
    response.glue_points.emplace_back(0.0f, 0.0f);
    response.glue_points.emplace_back(20.0f, 0.0f);
    response.preview_authority_ready = true;
    response.preview_authority_shared_with_execution = true;
    response.preview_spacing_valid = true;
    response.execution_plan = std::make_shared<DispensingPlan>(plan);

    auto execution_package = BuildMinimalExecutionPackage();
    execution_package.total_length_mm = response.total_length;
    execution_package.estimated_time_s = response.estimated_time;
    response.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(execution_package);
    return response;
}

std::shared_ptr<PlanningUseCase> CreateRealPlanningUseCase() {
    auto path_source = std::make_shared<LinePathSourceStub>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>(),
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(),
        std::make_shared<Siligen::Application::Services::Dispensing::DispensePlanningFacade>(),
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
    use_case.plans_[plan_id] = plan_record;
}

DispensingWorkflowUseCase CreateUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case = nullptr) {
    if (!execution_use_case) {
        execution_use_case = MakeDummyShared<DispensingExecutionUseCase>();
    }
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        MakeDummyShared<PlanningUseCase>(),
        execution_use_case,
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
        MakeDummyShared<DispensingExecutionUseCase>(),
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

std::shared_ptr<DispensingExecutionUseCase> CreateRuntimeExecutionUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port) {
    return std::make_shared<DispensingExecutionUseCase>(
        nullptr,
        nullptr,
        motion_state_port,
        connection_port,
        nullptr,
        nullptr,
        nullptr,
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
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    plan_record.preview_snapshot_id = plan_id;
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
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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

TEST(DispensingWorkflowUseCaseTest, CompatibilityServiceBuildPlanningRequestMapsLegacyFieldsAndOverrideSourcePath) {
    DispensingExecutionCompatibilityService service;
    auto request = BuildLegacyRequest("original.dxf");

    auto result = service.BuildPlanningRequest(request, "prepared.pb");
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& planning_request = result.Value();
    EXPECT_EQ(planning_request.dxf_filepath, "prepared.pb");
    EXPECT_EQ(planning_request.optimize_path, request.optimize_path);
    EXPECT_FLOAT_EQ(planning_request.start_x, request.start_x);
    EXPECT_FLOAT_EQ(planning_request.start_y, request.start_y);
    EXPECT_EQ(planning_request.approximate_splines, request.approximate_splines);
    EXPECT_EQ(planning_request.two_opt_iterations, request.two_opt_iterations);
    EXPECT_FLOAT_EQ(planning_request.spline_max_step_mm, request.spline_max_step_mm);
    EXPECT_FLOAT_EQ(planning_request.spline_max_error_mm, request.spline_max_error_mm);
    EXPECT_FLOAT_EQ(planning_request.continuity_tolerance_mm, request.continuity_tolerance_mm);
    EXPECT_FLOAT_EQ(planning_request.curve_chain_angle_deg, request.curve_chain_angle_deg);
    EXPECT_FLOAT_EQ(planning_request.curve_chain_max_segment_mm, request.curve_chain_max_segment_mm);
    EXPECT_EQ(planning_request.use_hardware_trigger, request.use_hardware_trigger);
    EXPECT_EQ(planning_request.use_interpolation_planner, request.use_interpolation_planner);
    EXPECT_EQ(planning_request.interpolation_algorithm, request.interpolation_algorithm);
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.max_velocity, request.dry_run_speed_mm_s.value());
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.max_acceleration, request.acceleration_mm_s2.value());
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.arc_tolerance, request.arc_tolerance_mm);
    EXPECT_FLOAT_EQ(planning_request.trajectory_config.max_jerk, request.max_jerk);
}

TEST(DispensingWorkflowUseCaseTest, CompatibilityServiceRejectsMissingExecutionPlan) {
    DispensingExecutionCompatibilityService service;
    PlanningResponse planning_response;
    auto request = BuildLegacyRequest();

    auto result = service.BuildExecutionRequest(planning_response, request, "artifact.pb");
    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("missing execution package"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, CompatibilityServiceBuildsCanonicalExecutionRequest) {
    DispensingExecutionCompatibilityService service;
    auto request = BuildLegacyRequest("legacy-path.dxf");
    auto planning_response = BuildPlanningResponseWithExecutionPlan();

    auto result = service.BuildExecutionRequest(planning_response, request, "artifact.pb");
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    const auto& execution_request = result.Value();
    EXPECT_EQ(execution_request.source_path, "artifact.pb");
    EXPECT_EQ(execution_request.use_hardware_trigger, request.use_hardware_trigger);
    EXPECT_EQ(execution_request.dry_run, request.dry_run);
    EXPECT_EQ(execution_request.max_jerk, request.max_jerk);
    EXPECT_EQ(execution_request.arc_tolerance_mm, request.arc_tolerance_mm);
    ASSERT_TRUE(execution_request.dispensing_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(execution_request.dispensing_speed_mm_s.value(), request.dispensing_speed_mm_s.value());
    ASSERT_TRUE(execution_request.dry_run_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(execution_request.dry_run_speed_mm_s.value(), request.dry_run_speed_mm_s.value());
    ASSERT_TRUE(execution_request.acceleration_mm_s2.has_value());
    EXPECT_FLOAT_EQ(execution_request.acceleration_mm_s2.value(), request.acceleration_mm_s2.value());
    EXPECT_TRUE(execution_request.velocity_trace_enabled);
    EXPECT_EQ(execution_request.velocity_trace_interval_ms, request.velocity_trace_interval_ms);
    EXPECT_EQ(execution_request.velocity_trace_path, request.velocity_trace_path);
    EXPECT_TRUE(execution_request.velocity_guard_stop_on_violation);
    EXPECT_GE(execution_request.execution_package.execution_plan.interpolation_points.size(), 2U);
}

TEST(DispensingWorkflowUseCaseTest, ExecutionWorkflowUseCasePlansThenDelegatesToExecutionLayer) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
    auto execution_use_case = std::make_shared<DispensingExecutionUseCase>(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    DispensingExecutionWorkflowUseCase use_case(planning_use_case, execution_use_case);

    auto request = BuildLegacyRequest(temp_pb_file.string());
    auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::PORT_NOT_INITIALIZED);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanStoresCanonicalExecutionRequestAndArtifactSourcePath) {
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
    request.execution_request = BuildLegacyRequest("legacy-input.dxf");

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
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsUnhomedAxis) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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

    auto runtime_context = std::make_shared<JobExecutionContext>();
    runtime_context->job_id = "job-1";
    runtime_context->plan_id = "plan-1";
    runtime_context->plan_fingerprint = "fp-plan-1";
    runtime_context->state.store(JobState::PAUSED);
    runtime_context->pause_requested.store(true);
    execution_use_case->RegisterJobContextForTesting(runtime_context);
    execution_use_case->SetActiveJobForTesting("job-1");

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

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsPreviewAuthorityMismatchBeforeRuntimeLaunch) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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

TEST(DispensingWorkflowUseCaseTest, FinalizeJobClearsConfirmedPreviewState) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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
    use_case.plans_[plan_record.response.plan_id] = plan_record;
    use_case.job_plan_index_["job-finish"] = "plan-finish";

    auto runtime_context = std::make_shared<JobExecutionContext>();
    runtime_context->job_id = "job-finish";
    runtime_context->plan_id = "plan-finish";
    runtime_context->plan_fingerprint = "fp-plan-finish";
    runtime_context->target_count.store(1);
    runtime_context->state.store(JobState::RUNNING);
    execution_use_case->RegisterJobContextForTesting(runtime_context);
    execution_use_case->SetActiveJobForTesting("job-finish");

    auto stop_result = use_case.StopJob(runtime_context->job_id);
    ASSERT_TRUE(stop_result.IsSuccess());

    ASSERT_TRUE(use_case.plans_.find("plan-finish") != use_case.plans_.end());
    EXPECT_EQ(
        use_case.plans_.at("plan-finish").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY);
    EXPECT_TRUE(use_case.plans_.at("plan-finish").confirmed_at.empty());
}
