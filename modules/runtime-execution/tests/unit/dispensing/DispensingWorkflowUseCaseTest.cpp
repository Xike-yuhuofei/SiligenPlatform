#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef SILIGEN_TEST_HOOKS
#define SILIGEN_TEST_HOOKS
#endif
#define private public
#include "dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/dispensing/IProductionBaselinePort.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessPath.h"
#undef private
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRequest;
using Siligen::Application::UseCases::Dispensing::PreparePlanResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRuntimeOverrides;
using Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;
using Siligen::Application::UseCases::Dispensing::StartJobResponse;
using Siligen::Application::Ports::Dispensing::IProductionBaselinePort;
using Siligen::Application::Ports::Dispensing::ResolvedProductionBaseline;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::JobIngest::Contracts::DxfImportDiagnostics;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::Device::Contracts::Commands::DeviceConnection;
using Siligen::Device::Contracts::Ports::DispenserDevicePort;
using Siligen::Device::Contracts::Ports::DeviceConnectionPort;
using Siligen::Device::Contracts::Ports::MotionDevicePort;
using Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using Siligen::Device::Contracts::State::DeviceConnectionState;
using Siligen::Device::Contracts::State::DeviceSession;
using Siligen::Device::Contracts::State::DispenserState;
using Siligen::Device::Contracts::State::HeartbeatSnapshot;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Motion::Ports::InterpolationData;
using Siligen::Domain::Motion::Ports::HomingState;
using Siligen::Domain::Motion::Ports::HomingStatus;
using Siligen::Domain::Motion::Ports::IHomingPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint;
using Siligen::Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Domain::Safety::ValueObjects::InterlockSignals;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationType;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::PointFlyingCarrierDirectionMode;
using Siligen::Shared::Types::PointFlyingCarrierPolicy;
using Siligen::Shared::Types::Point2D;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;
using RuntimeDispensingProcessPort = Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort;

constexpr char kCanonicalRecipeId[] = "recipe-preview";
constexpr char kCanonicalVersionId[] = "version-published";

template <typename T>
std::shared_ptr<T> MakeDummyShared() {
    return std::shared_ptr<T>(reinterpret_cast<T*>(0x1), [](T*) {});
}

class RuntimeWorkflowExecutionPortAdapter final
    : public Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort {
public:
    explicit RuntimeWorkflowExecutionPortAdapter(std::shared_ptr<DispensingExecutionUseCase> use_case)
        : use_case_(std::move(use_case)) {
        if (!use_case_) {
            throw std::invalid_argument("RuntimeWorkflowExecutionPortAdapter: use_case cannot be null");
        }
    }

    Result<Siligen::Application::Ports::Dispensing::WorkflowJobId> StartJob(
        const Siligen::Application::Ports::Dispensing::WorkflowRuntimeStartJobRequest& request) override {
        Siligen::Application::UseCases::Dispensing::RuntimeStartJobRequest runtime_request;
        runtime_request.plan_id = request.plan_id;
        runtime_request.plan_fingerprint = request.plan_fingerprint;
        runtime_request.target_count = request.target_count;
        runtime_request.execution_request.execution_package = request.execution_request.execution_package;
        runtime_request.execution_request.source_path = request.execution_request.source_path;
        runtime_request.execution_request.use_hardware_trigger = request.execution_request.use_hardware_trigger;
        runtime_request.execution_request.dry_run = request.execution_request.dry_run;
        runtime_request.execution_request.machine_mode = request.execution_request.machine_mode;
        runtime_request.execution_request.execution_mode = request.execution_request.execution_mode;
        runtime_request.execution_request.output_policy = request.execution_request.output_policy;
        runtime_request.execution_request.max_jerk = request.execution_request.max_jerk;
        runtime_request.execution_request.arc_tolerance_mm = request.execution_request.arc_tolerance_mm;
        runtime_request.execution_request.dispensing_speed_mm_s = request.execution_request.dispensing_speed_mm_s;
        runtime_request.execution_request.dry_run_speed_mm_s = request.execution_request.dry_run_speed_mm_s;
        runtime_request.execution_request.rapid_speed_mm_s = request.execution_request.rapid_speed_mm_s;
        runtime_request.execution_request.acceleration_mm_s2 = request.execution_request.acceleration_mm_s2;
        runtime_request.execution_request.velocity_trace_enabled = request.execution_request.velocity_trace_enabled;
        runtime_request.execution_request.velocity_trace_interval_ms =
            request.execution_request.velocity_trace_interval_ms;
        runtime_request.execution_request.velocity_trace_path = request.execution_request.velocity_trace_path;
        runtime_request.execution_request.velocity_guard_enabled = request.execution_request.velocity_guard_enabled;
        runtime_request.execution_request.velocity_guard_ratio = request.execution_request.velocity_guard_ratio;
        runtime_request.execution_request.velocity_guard_abs_mm_s = request.execution_request.velocity_guard_abs_mm_s;
        runtime_request.execution_request.velocity_guard_min_expected_mm_s =
            request.execution_request.velocity_guard_min_expected_mm_s;
        runtime_request.execution_request.velocity_guard_grace_ms = request.execution_request.velocity_guard_grace_ms;
        runtime_request.execution_request.velocity_guard_interval_ms =
            request.execution_request.velocity_guard_interval_ms;
        runtime_request.execution_request.velocity_guard_max_consecutive =
            request.execution_request.velocity_guard_max_consecutive;
        runtime_request.execution_request.velocity_guard_stop_on_violation =
            request.execution_request.velocity_guard_stop_on_violation;
        return use_case_->StartJob(runtime_request);
    }

private:
    std::shared_ptr<DispensingExecutionUseCase> use_case_;
};

std::shared_ptr<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort> CreateRuntimeWorkflowExecutionPort(
    std::shared_ptr<DispensingExecutionUseCase> use_case) {
    return std::make_shared<RuntimeWorkflowExecutionPortAdapter>(std::move(use_case));
}

class LinePathSourceStub final : public Siligen::ProcessPath::Contracts::IPathSourcePort {
   public:
    Result<Siligen::ProcessPath::Contracts::PathSourceResult> LoadFromFile(const std::string&) override {
        using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
        using Siligen::ProcessPath::Contracts::PathSourceResult;
        using Siligen::ProcessPath::Contracts::Primitive;

        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(20.0f, 0.0f)));
        result.metadata.push_back(PathPrimitiveMeta{});
        return Result<PathSourceResult>::Success(result);
    }
};

class SlowCountingLinePathSourceStub final : public Siligen::ProcessPath::Contracts::IPathSourcePort {
   public:
    explicit SlowCountingLinePathSourceStub(
        std::shared_ptr<std::atomic<int>> load_calls,
        std::chrono::milliseconds delay = std::chrono::milliseconds(75))
        : load_calls_(std::move(load_calls)), delay_(delay) {}

    Result<Siligen::ProcessPath::Contracts::PathSourceResult> LoadFromFile(const std::string&) override {
        load_calls_->fetch_add(1);
        std::this_thread::sleep_for(delay_);
        using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
        using Siligen::ProcessPath::Contracts::PathSourceResult;
        using Siligen::ProcessPath::Contracts::Primitive;

        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(20.0f, 0.0f)));
        result.metadata.push_back(PathPrimitiveMeta{});
        return Result<PathSourceResult>::Success(result);
    }

   private:
    std::shared_ptr<std::atomic<int>> load_calls_;
    std::chrono::milliseconds delay_;
};

class SlowExportPortStub final : public IPlanningArtifactExportPort {
   public:
    explicit SlowExportPortStub(
        std::shared_ptr<std::atomic<int>> export_calls,
        std::chrono::milliseconds delay = std::chrono::milliseconds(120))
        : export_calls_(std::move(export_calls)), delay_(delay) {}

    Result<PlanningArtifactExportResult> Export(const PlanningArtifactExportRequest&) override {
        export_calls_->fetch_add(1);
        std::this_thread::sleep_for(delay_);
        PlanningArtifactExportResult result;
        result.export_requested = true;
        result.success = true;
        return Result<PlanningArtifactExportResult>::Success(result);
    }

   private:
    std::shared_ptr<std::atomic<int>> export_calls_;
    std::chrono::milliseconds delay_;
};

PointFlyingCarrierPolicy BuildCanonicalPointFlyingCarrierPolicy() {
    PointFlyingCarrierPolicy policy;
    policy.direction_mode = PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    policy.trigger_spatial_interval_mm = 5.0f;
    return policy;
}

DxfImportDiagnostics BuildProductionReadyImportDiagnostics() {
    DxfImportDiagnostics diagnostics;
    diagnostics.result_classification = "success";
    diagnostics.preview_ready = true;
    diagnostics.production_ready = true;
    diagnostics.summary = "DXF import succeeded and is ready for production.";
    diagnostics.resolved_units = "mm";
    diagnostics.resolved_unit_scale = 1.0;
    return diagnostics;
}

void SeedProductionReadyImportDiagnostics(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    plan_record.response.import_diagnostics = BuildProductionReadyImportDiagnostics();
}

void SeedProductionReadyImportDiagnostics(DispensingWorkflowUseCase::ArtifactRecord& artifact_record) {
    const auto diagnostics = BuildProductionReadyImportDiagnostics();
    artifact_record.response.import_diagnostics = diagnostics;
    artifact_record.upload_response.import_diagnostics = diagnostics;
}

class FakeProductionBaselinePort final : public IProductionBaselinePort {
   public:
    Result<ResolvedProductionBaseline> ResolveCurrentBaseline() const override {
        ResolvedProductionBaseline resolved;
        resolved.baseline_id = "test-production-baseline";
        resolved.baseline_fingerprint = baseline_fingerprint;
        resolved.point_flying_carrier_policy = policy;
        return Result<ResolvedProductionBaseline>::Success(std::move(resolved));
    }

    PointFlyingCarrierPolicy policy = BuildCanonicalPointFlyingCarrierPolicy();
    std::string baseline_fingerprint = "baseline-fingerprint";
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
    request.interpolation_algorithm = Siligen::MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
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

PreparePlanRequest BuildCanonicalPreparePlanRequest(const std::string& artifact_id) {
    PreparePlanRequest request;
    request.artifact_id = artifact_id;
    request.planning_request = BuildCanonicalPlanningRequest();
    request.planning_request.dxf_filepath.clear();
    request.runtime_overrides = BuildPreparePlanRuntimeOverrides();
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

bool SpinUntil(
    const std::function<bool()>& predicate,
    std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) {
    const auto start = std::chrono::steady_clock::now();
    while (!predicate()) {
        if (std::chrono::steady_clock::now() - start >= timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return true;
}

std::string GluePointsFingerprint(const std::vector<Point2D>& points) {
    std::ostringstream oss;
    for (const auto& point : points) {
        oss << point.x << ',' << point.y << ';';
    }
    return oss.str();
}

std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> CreatePlanningOperations() {
    return Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider{}
        .CreateOperations();
}

std::shared_ptr<PlanningUseCase> CreateRealPlanningUseCase() {
    auto path_source = std::make_shared<LinePathSourceStub>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        nullptr,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service));
}

std::shared_ptr<PlanningUseCase> CreatePlanningUseCaseWithPathSourceAndExport(
    const std::shared_ptr<Siligen::ProcessPath::Contracts::IPathSourcePort>& path_source,
    const std::shared_ptr<IPlanningArtifactExportPort>& export_port) {
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        nullptr,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service),
        export_port);
}

std::shared_ptr<PlanningUseCase> CreatePlanningUseCaseWithPathSource(
    const std::shared_ptr<Siligen::ProcessPath::Contracts::IPathSourcePort>& path_source) {
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        nullptr,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service));
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

class FakeMotionDevicePort final : public MotionDevicePort {
   public:
    FakeMotionDevicePort() {
        capabilities.trigger.supports_position_trigger = true;
        capabilities.trigger.supports_time_trigger = true;
        capabilities.trigger.supports_in_motion_position_trigger = true;
        capabilities.trigger.supports_in_motion_time_trigger = true;
    }

    Siligen::SharedKernel::VoidResult Connect(const DeviceConnection&) override {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    Siligen::SharedKernel::VoidResult Disconnect() override {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    Siligen::SharedKernel::Result<DeviceSession> ReadSession() const override {
        return Siligen::SharedKernel::Result<DeviceSession>::Success(DeviceSession{});
    }
    Siligen::SharedKernel::VoidResult Execute(const Siligen::Device::Contracts::Commands::MotionCommand&) override {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState> ReadState() const override {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState>::Success(
            Siligen::Device::Contracts::State::MotionState{});
    }
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>
    DescribeCapabilities() const override {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>::Success(
            capabilities);
    }
    Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>> ReadFaults()
        const override {
        return Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>>::Success(
            {});
    }

    Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities{};
};

class FakeDispenserDevicePort final : public DispenserDevicePort {
   public:
    FakeDispenserDevicePort() {
        capability.supports_prime = true;
        capability.supports_pause = true;
        capability.supports_resume = true;
        capability.supports_continuous_mode = true;
        capability.supports_in_motion_pulse_shot = true;
    }

    Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Device::Contracts::Commands::DispenserCommand&) override {
        return Siligen::SharedKernel::VoidResult::Success();
    }
    Siligen::SharedKernel::Result<DispenserState> ReadState() const override {
        return Siligen::SharedKernel::Result<DispenserState>::Success(DispenserState{});
    }
    Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>
    DescribeCapability() const override {
        return Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DispenserCapability>::Success(
            capability);
    }

    Siligen::Device::Contracts::Capabilities::DispenserCapability capability{};
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

class FakeDispensingProcessPort final : public RuntimeDispensingProcessPort {
   public:
    Result<void> ValidateHardwareConnection() noexcept override {
        return Result<void>::Success();
    }

    Result<DispensingRuntimeParams> BuildRuntimeParams(const DispensingRuntimeOverrides& overrides) noexcept override {
        DispensingRuntimeParams params;
        params.dispensing_velocity =
            overrides.dispensing_speed_mm_s.value_or(overrides.dry_run_speed_mm_s.value_or(0.0f));
        params.rapid_velocity = overrides.rapid_speed_mm_s.value_or(0.0f);
        params.acceleration = overrides.acceleration_mm_s2.value_or(0.0f);
        params.velocity_guard_enabled = overrides.velocity_guard_enabled;
        params.velocity_guard_ratio = overrides.velocity_guard_ratio;
        params.velocity_guard_abs_mm_s = overrides.velocity_guard_abs_mm_s;
        params.velocity_guard_min_expected_mm_s = overrides.velocity_guard_min_expected_mm_s;
        params.velocity_guard_grace_ms = overrides.velocity_guard_grace_ms;
        params.velocity_guard_interval_ms = overrides.velocity_guard_interval_ms;
        params.velocity_guard_max_consecutive = overrides.velocity_guard_max_consecutive;
        params.velocity_guard_stop_on_violation = overrides.velocity_guard_stop_on_violation;
        return Result<DispensingRuntimeParams>::Success(params);
    }

    Result<DispensingExecutionReport> ExecuteProcess(
        const DispensingExecutionPlan& plan,
        const DispensingRuntimeParams&,
        const DispensingExecutionOptions&,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        IDispensingExecutionObserver* observer = nullptr) noexcept override {
        DispensingExecutionReport report;
        report.total_distance = plan.total_length_mm;
        const uint32 total_segments = !plan.interpolation_segments.empty()
                                          ? static_cast<uint32>(plan.interpolation_segments.size())
                                          : (plan.interpolation_points.size() > 1U
                                                 ? static_cast<uint32>(plan.interpolation_points.size() - 1U)
                                                 : 0U);

        if (observer != nullptr) {
            observer->OnMotionStart();
        }

        for (uint32 index = 0; index < total_segments; ++index) {
            if (stop_flag != nullptr && stop_flag->load()) {
                if (observer != nullptr) {
                    observer->OnMotionStop();
                }
                return Result<DispensingExecutionReport>::Failure(
                    Siligen::Shared::Types::Error(
                        ErrorCode::INVALID_STATE,
                        "execution cancelled",
                        "FakeDispensingProcessPort"));
            }

            while (pause_flag != nullptr && pause_flag->load()) {
                if (pause_applied_flag != nullptr) {
                    pause_applied_flag->store(true);
                }
                if (observer != nullptr) {
                    observer->OnPauseStateChanged(true);
                }
                if (stop_flag != nullptr && stop_flag->load()) {
                    if (pause_applied_flag != nullptr) {
                        pause_applied_flag->store(false);
                    }
                    if (observer != nullptr) {
                        observer->OnPauseStateChanged(false);
                        observer->OnMotionStop();
                    }
                    return Result<DispensingExecutionReport>::Failure(
                        Siligen::Shared::Types::Error(
                            ErrorCode::INVALID_STATE,
                            "execution cancelled",
                            "FakeDispensingProcessPort"));
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            if (pause_applied_flag != nullptr) {
                pause_applied_flag->store(false);
            }
            if (observer != nullptr) {
                observer->OnPauseStateChanged(false);
                observer->OnProgress(index + 1U, total_segments);
            }
            report.executed_segments = index + 1U;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        if (observer != nullptr) {
            observer->OnMotionStop();
        }
        return Result<DispensingExecutionReport>::Success(report);
    }

    void StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag = nullptr) noexcept override {
        if (stop_flag != nullptr) {
            stop_flag->store(true);
        }
        if (pause_flag != nullptr) {
            pause_flag->store(false);
        }
    }
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

void SeedAuthorityTriggerPoints(
    DispensingWorkflowUseCase::PlanRecord& plan_record,
    const std::vector<Point2D>& points) {
    plan_record.authority_trigger_layout.trigger_points.clear();
    plan_record.authority_trigger_layout.trigger_points.reserve(points.size());
    float cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm += static_cast<float>(points[index - 1U].DistanceTo(points[index]));
        }
        Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint trigger_point;
        trigger_point.trigger_id = "trigger-" + std::to_string(index);
        trigger_point.layout_ref = plan_record.authority_trigger_layout.layout_id;
        trigger_point.sequence_index_global = index;
        trigger_point.distance_mm_global = cumulative_length_mm;
        trigger_point.position = points[index];
        plan_record.authority_trigger_layout.trigger_points.push_back(trigger_point);
    }
    plan_record.response.total_length_mm = cumulative_length_mm;
}

void SeedPlan(DispensingWorkflowUseCase& use_case, const std::string& plan_id) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    SeedProductionReadyImportDiagnostics(plan_record);
    plan_record.execution_launch.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            BuildMinimalExecutionPackage());
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
    plan_record.execution_authority_shared_with_execution = true;
    plan_record.execution_binding_ready = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    SeedAuthorityMetadata(plan_record, "layout-" + plan_id);
    plan_record.execution_assembly.success = true;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.execution_assembly.preview_authority_shared_with_execution = true;
    plan_record.execution_assembly.execution_binding_ready = true;
    plan_record.execution_assembly.execution_package = plan_record.execution_launch.execution_package;
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    SeedAuthorityTriggerPoints(plan_record, plan_record.glue_points);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    use_case.plans_[plan_id] = plan_record;
}

void ConfigurePointFlyingShotPlan(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    ExecutionPackageBuilt built;
    built.execution_plan.geometry_kind = DispensingExecutionGeometryKind::POINT;
    built.execution_plan.execution_strategy = DispensingExecutionStrategy::FLYING_SHOT;
    built.execution_plan.trigger_distances_mm = {0.0f};
    built.execution_plan.trigger_interval_mm = 3.0f;
    built.execution_plan.total_length_mm = 3.0f;
    built.total_length_mm = 3.0f;
    built.estimated_time_s = 0.12f;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "point-flying";

    MotionTrajectoryPoint start_point;
    start_point.t = 0.0f;
    start_point.position = {5.0f, 5.0f, 0.0f};
    start_point.velocity = {25.0f, 0.0f, 0.0f};
    start_point.dispense_on = true;

    MotionTrajectoryPoint end_point;
    end_point.t = 0.12f;
    end_point.position = {8.0f, 5.0f, 0.0f};
    end_point.velocity = {0.0f, 0.0f, 0.0f};
    end_point.dispense_on = true;

    built.execution_plan.motion_trajectory.points = {start_point, end_point};
    built.execution_plan.motion_trajectory.total_length = 3.0f;
    built.execution_plan.motion_trajectory.total_time = 0.12f;

    Siligen::TrajectoryPoint preview_start;
    preview_start.position = Point2D{5.0f, 5.0f};
    preview_start.sequence_id = 0U;
    preview_start.timestamp = 0.0f;
    preview_start.velocity = 25.0f;
    preview_start.enable_position_trigger = true;
    preview_start.trigger_position_mm = 0.0f;

    Siligen::TrajectoryPoint preview_end;
    preview_end.position = Point2D{8.0f, 5.0f};
    preview_end.sequence_id = 1U;
    preview_end.timestamp = 0.12f;
    preview_end.velocity = 0.0f;

    built.execution_plan.interpolation_points = {preview_start, preview_end};

    InterpolationData segment;
    segment.type = InterpolationType::LINEAR;
    segment.positions = {8.0f, 5.0f};
    segment.velocity = 25.0f;
    segment.acceleration = 100.0f;
    segment.end_velocity = 0.0f;
    built.execution_plan.interpolation_segments.push_back(segment);

    auto package = std::make_shared<ExecutionPackageValidated>(std::move(built));
    plan_record.execution_launch.execution_package = package;
    plan_record.execution_assembly.execution_package = package;
    plan_record.execution_trajectory_points = package->execution_plan.interpolation_points;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.glue_points = {Point2D{5.0f, 5.0f}};
    plan_record.response.total_length_mm = 3.0f;
    plan_record.authority_trigger_layout.trigger_points.clear();
    SeedAuthorityTriggerPoints(plan_record, plan_record.glue_points);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
}

DispensingWorkflowUseCase CreateUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case = nullptr,
    std::shared_ptr<FakeProductionBaselinePort> recipe_planning_policy_port =
        std::make_shared<FakeProductionBaselinePort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    auto execution_port = execution_use_case
        ? CreateRuntimeWorkflowExecutionPort(std::move(execution_use_case))
        : MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        MakeDummyShared<PlanningUseCase>(),
        recipe_planning_policy_port,
        execution_port,
        connection_port,
        motion_device_port,
        dispenser_device_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase CreateUseCaseWithPlanning(
    const std::shared_ptr<PlanningUseCase>& planning_use_case,
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<FakeProductionBaselinePort> recipe_planning_policy_port =
        std::make_shared<FakeProductionBaselinePort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        recipe_planning_policy_port,
        MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>(),
        connection_port,
        motion_device_port,
        dispenser_device_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase CreateUseCaseWithPlanningAndExecution(
    const std::shared_ptr<PlanningUseCase>& planning_use_case,
    const std::shared_ptr<DispensingExecutionUseCase>& execution_use_case,
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<FakeProductionBaselinePort> recipe_planning_policy_port =
        std::make_shared<FakeProductionBaselinePort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        recipe_planning_policy_port,
        CreateRuntimeWorkflowExecutionPort(execution_use_case),
        connection_port,
        motion_device_port,
        dispenser_device_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

std::shared_ptr<DispensingExecutionUseCase> CreateRuntimeExecutionUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port) {
    auto process_port = std::make_shared<FakeDispensingProcessPort>();
    return std::make_shared<DispensingExecutionUseCase>(
        nullptr,
        nullptr,
        motion_state_port,
        connection_port,
        nullptr,
        process_port,
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
    SeedProductionReadyImportDiagnostics(plan_record);
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_binding_ready = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_diagnostic_code.clear();
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    plan_record.preview_snapshot_id = plan_id;
    SeedAuthorityMetadata(plan_record, "layout-" + plan_id);
    for (const auto& point : points) {
        plan_record.execution_trajectory_points.emplace_back(point.x, point.y, 0.0f);
        plan_record.execution_assembly.motion_trajectory_points.emplace_back(point.x, point.y, 0.0f);
        plan_record.glue_points.push_back(point);
    }
    SeedAuthorityTriggerPoints(plan_record, plan_record.glue_points);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    return plan_record;
}

bool SnapshotContainsPoint(
    const Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.motion_preview_polyline) {
        if (std::abs(point.x - target_x) <= tolerance && std::abs(point.y - target_y) <= tolerance) {
            return true;
        }
    }
    return false;
}

bool MotionPreviewContainsPoint(
    const Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse& snapshot,
    float target_x,
    float target_y,
    float tolerance = 1e-3f) {
    for (const auto& point : snapshot.motion_preview_polyline) {
        if (std::abs(point.x - target_x) <= tolerance && std::abs(point.y - target_y) <= tolerance) {
            return true;
        }
    }
    return false;
}

bool MotionPreviewHasUnexpectedSegmentAngle(
    const Siligen::Application::UseCases::Dispensing::PreviewSnapshotResponse& snapshot,
    const std::vector<double>& expected_angles_deg,
    double tolerance_deg = 0.5) {
    constexpr double kRadiansToDegrees = 180.0 / 3.14159265358979323846;
    for (std::size_t i = 1; i < snapshot.motion_preview_polyline.size(); ++i) {
        const auto& prev = snapshot.motion_preview_polyline[i - 1U];
        const auto& curr = snapshot.motion_preview_polyline[i];
        const double dx = static_cast<double>(curr.x) - static_cast<double>(prev.x);
        const double dy = static_cast<double>(curr.y) - static_cast<double>(prev.y);
        if (std::abs(dx) <= 1e-9 && std::abs(dy) <= 1e-9) {
            continue;
        }

        const double angle_deg = std::atan2(dy, dx) * kRadiansToDegrees;
        bool matched = false;
        for (const auto expected_angle_deg : expected_angles_deg) {
            if (std::abs(angle_deg - expected_angle_deg) <= tolerance_deg) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            return true;
        }
    }
    return false;
}

Siligen::ProcessPath::Contracts::ProcessSegment BuildLineProcessSegment(
    const Point2D& start,
    const Point2D& end) {
    Siligen::ProcessPath::Contracts::Segment segment;
    segment.type = Siligen::ProcessPath::Contracts::SegmentType::Line;
    segment.line.start = start;
    segment.line.end = end;
    segment.length = start.DistanceTo(end);

    Siligen::ProcessPath::Contracts::ProcessSegment process_segment;
    process_segment.geometry = segment;
    process_segment.dispense_on = true;
    return process_segment;
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
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

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
    EXPECT_FALSE(stored_launch.execution_package);
    EXPECT_TRUE(stored_launch.authority_preview.success);
    EXPECT_FALSE(stored_launch.authority_preview.artifacts.glue_points.empty());
    EXPECT_FALSE(stored_launch.authority_cache_key.empty());
    EXPECT_EQ(prepared.filepath, temp_pb_file.string());
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanUsesCurrentProductionBaselineWithoutRecipeVersion) {
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
    artifact_record.response.artifact_id = "artifact-missing-recipe-version";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    const auto result = use_case.PreparePlan(BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().plan_fingerprint.empty());
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanReusesLatestPlanForIdenticalAuthorityAndRuntimeFingerprint) {
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
    artifact_record.response.artifact_id = "artifact-cache-hit";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto first = use_case.PreparePlan(request);
    ASSERT_TRUE(first.IsSuccess()) << first.GetError().ToString();
    auto& stored_record = use_case.plans_.at(first.Value().plan_id);
    stored_record.response.segment_count = 999U;
    stored_record.response.point_count = 999U;
    stored_record.response.total_length_mm = 999.0f;
    stored_record.response.estimated_time_s = 999.0f;

    const auto second = use_case.PreparePlan(request);
    ASSERT_TRUE(second.IsSuccess()) << second.GetError().ToString();
    EXPECT_EQ(first.Value().plan_id, second.Value().plan_id);
    EXPECT_EQ(use_case.plans_.size(), 1U);
    EXPECT_EQ(second.Value().segment_count, first.Value().segment_count);
    EXPECT_EQ(second.Value().point_count, first.Value().point_count);
    EXPECT_FLOAT_EQ(second.Value().total_length_mm, first.Value().total_length_mm);
    EXPECT_FLOAT_EQ(second.Value().estimated_time_s, first.Value().estimated_time_s);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanSingleFlightsConcurrentAuthorityPreviewRequests) {
    ScopedTempPbFile temp_pb_file;
    auto load_calls = std::make_shared<std::atomic<int>>(0);
    auto planning_use_case =
        CreatePlanningUseCaseWithPathSource(std::make_shared<SlowCountingLinePathSourceStub>(load_calls));
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
    artifact_record.response.artifact_id = "artifact-single-flight";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    Result<PreparePlanResponse> first_result;
    Result<PreparePlanResponse> second_result;
    std::thread first_thread([&]() { first_result = use_case.PreparePlan(request); });
    std::thread second_thread([&]() { second_result = use_case.PreparePlan(request); });
    first_thread.join();
    second_thread.join();

    ASSERT_TRUE(first_result.IsSuccess()) << first_result.GetError().ToString();
    ASSERT_TRUE(second_result.IsSuccess()) << second_result.GetError().ToString();
    EXPECT_EQ(load_calls->load(), 1);
    EXPECT_EQ(use_case.plans_.size(), 1U);
    EXPECT_EQ(first_result.Value().plan_id, second_result.Value().plan_id);
    EXPECT_GT(first_result.Value().performance_profile.prepare_total_ms, 0U);
    EXPECT_GT(second_result.Value().performance_profile.prepare_total_ms, 0U);
    EXPECT_TRUE(
        first_result.Value().performance_profile.authority_joined_inflight ||
        second_result.Value().performance_profile.authority_joined_inflight ||
        first_result.Value().performance_profile.authority_cache_hit ||
        second_result.Value().performance_profile.authority_cache_hit);
}

TEST(DispensingWorkflowUseCaseTest, StartJobReturnsStructuredResponseWithExecutionProfile) {
    ScopedTempPbFile temp_pb_file;
    auto export_calls = std::make_shared<std::atomic<int>>(0);
    auto planning_use_case = CreatePlanningUseCaseWithPathSourceAndExport(
        std::make_shared<LinePathSourceStub>(),
        std::make_shared<SlowExportPortStub>(export_calls, std::chrono::milliseconds(10)));
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
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-start-profile";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = prepare_result.Value().plan_id;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest confirm_request;
    confirm_request.plan_id = prepare_result.Value().plan_id;
    confirm_request.snapshot_hash = snapshot_result.Value().snapshot_hash;
    const auto confirm_result = use_case.ConfirmPreview(confirm_request);
    ASSERT_TRUE(confirm_result.IsSuccess()) << confirm_result.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::StartJobRequest start_request;
    start_request.plan_id = prepare_result.Value().plan_id;
    start_request.plan_fingerprint = prepare_result.Value().plan_fingerprint;
    start_request.target_count = 2;
    const auto start_result = use_case.StartJob(start_request);

    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().ToString();
    const auto& response = start_result.Value();
    EXPECT_TRUE(response.started);
    EXPECT_FALSE(response.job_id.empty());
    EXPECT_EQ(response.plan_id, prepare_result.Value().plan_id);
    EXPECT_EQ(response.plan_fingerprint, prepare_result.Value().plan_fingerprint);
    EXPECT_EQ(response.target_count, 2U);
    EXPECT_GE(response.performance_profile.motion_plan_ms, 0U);
    EXPECT_GE(response.performance_profile.assembly_ms, 0U);
    EXPECT_GE(response.performance_profile.export_ms, 0U);
    EXPECT_GT(response.performance_profile.execution_total_ms, 0U);
    EXPECT_EQ(export_calls->load(), 1);
}

TEST(DispensingWorkflowUseCaseTest, StartJobAllowsDryRunWithoutConnectedHardware) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    connection_port->connected = false;
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    auto homing_port = std::make_shared<FakeHomingPort>();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-dry-run-offline");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-dry-run-offline";
    request.plan_fingerprint = "fp-plan-dry-run-offline";
    request.target_count = 1;
    const auto start_result = use_case.StartJob(request);

    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().ToString();
    const auto& response = start_result.Value();
    EXPECT_TRUE(response.started);
    EXPECT_FALSE(response.job_id.empty());
    ASSERT_TRUE(use_case.plans_.find(request.plan_id) != use_case.plans_.end());
    EXPECT_EQ(use_case.plans_.at(request.plan_id).runtime_job_id, response.job_id);
    EXPECT_EQ(use_case.job_plan_index_.at(response.job_id), request.plan_id);

    const auto runtime_status_result = execution_use_case->GetJobStatus(response.job_id);
    ASSERT_TRUE(runtime_status_result.IsSuccess()) << runtime_status_result.GetError().ToString();
    EXPECT_EQ(runtime_status_result.Value().plan_id, request.plan_id);
    EXPECT_EQ(runtime_status_result.Value().plan_fingerprint, request.plan_fingerprint);
    EXPECT_TRUE(runtime_status_result.Value().dry_run);
}

TEST(DispensingWorkflowUseCaseTest, EnsureExecutionAssemblySingleFlightsConcurrentRequests) {
    ScopedTempPbFile temp_pb_file;
    auto export_calls = std::make_shared<std::atomic<int>>(0);
    auto planning_use_case = CreatePlanningUseCaseWithPathSourceAndExport(
        std::make_shared<LinePathSourceStub>(),
        std::make_shared<SlowExportPortStub>(export_calls));
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
    artifact_record.response.artifact_id = "artifact-exec-single-flight";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    const auto plan_id = prepare_result.Value().plan_id;
    const auto expected_fingerprint = prepare_result.Value().plan_fingerprint;
    const auto expected_layout_id = use_case.plans_.at(plan_id).authority_trigger_layout.layout_id;

    Result<Siligen::Application::UseCases::Dispensing::ExecutionAssemblyTestProbe> first_result;
    Result<Siligen::Application::UseCases::Dispensing::ExecutionAssemblyTestProbe> second_result;
    std::thread first_thread([&]() { first_result = use_case.EnsureExecutionAssemblyReadyForTesting(plan_id); });
    ASSERT_TRUE(SpinUntil([&]() { return export_calls->load() > 0; }, std::chrono::milliseconds(3000)))
        << "leader execution assembly did not enter export";
    std::thread second_thread([&]() { second_result = use_case.EnsureExecutionAssemblyReadyForTesting(plan_id); });
    first_thread.join();
    second_thread.join();

    ASSERT_TRUE(first_result.IsSuccess()) << first_result.GetError().ToString();
    ASSERT_TRUE(second_result.IsSuccess()) << second_result.GetError().ToString();
    EXPECT_EQ(export_calls->load(), 1);
    EXPECT_TRUE(
        first_result.Value().joined_inflight ||
        second_result.Value().joined_inflight ||
        first_result.Value().cache_hit ||
        second_result.Value().cache_hit);

    const auto waiter = first_result.Value().joined_inflight ? first_result.Value() : second_result.Value();
    EXPECT_TRUE(waiter.joined_inflight || waiter.cache_hit);
    if (waiter.joined_inflight) {
        EXPECT_GT(waiter.wait_ms, 0U);
    }
    EXPECT_EQ(first_result.Value().authority_layout_id, expected_layout_id);
    EXPECT_EQ(second_result.Value().authority_layout_id, expected_layout_id);
    EXPECT_EQ(first_result.Value().plan_fingerprint, expected_fingerprint);
    EXPECT_EQ(second_result.Value().plan_fingerprint, expected_fingerprint);
    EXPECT_TRUE(first_result.Value().execution_authority_shared_with_execution);
    EXPECT_TRUE(second_result.Value().execution_authority_shared_with_execution);
    EXPECT_TRUE(first_result.Value().execution_binding_ready);
    EXPECT_TRUE(second_result.Value().execution_binding_ready);
    EXPECT_TRUE(first_result.Value().has_execution_launch_package);
    EXPECT_TRUE(second_result.Value().has_execution_launch_package);
    EXPECT_TRUE(first_result.Value().has_execution_assembly_package);
    EXPECT_TRUE(second_result.Value().has_execution_assembly_package);
    EXPECT_GT(first_result.Value().plan_execution_trajectory_point_count, 0U);
    EXPECT_GT(first_result.Value().execution_launch_interpolation_segment_count, 0U);
    EXPECT_GT(first_result.Value().execution_launch_interpolation_point_count, 0U);
    EXPECT_GT(first_result.Value().execution_launch_motion_point_count, 0U);
    EXPECT_GT(first_result.Value().execution_assembly_trajectory_point_count, 0U);
    EXPECT_GT(first_result.Value().execution_assembly_interpolation_segment_count, 0U);
    EXPECT_GT(first_result.Value().execution_assembly_interpolation_point_count, 0U);
    EXPECT_GT(first_result.Value().execution_assembly_motion_point_count, 0U);
    EXPECT_TRUE(first_result.Value().execution_cache_contains_plan);
    EXPECT_GE(first_result.Value().execution_cache_entry_count, 1U);
    EXPECT_EQ(use_case.plans_.at(plan_id).response.plan_fingerprint, expected_fingerprint);
    EXPECT_EQ(use_case.plans_.at(plan_id).authority_trigger_layout.layout_id, expected_layout_id);
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotResolvesExecutionAssemblyForPreparedPlan) {
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
    artifact_record.response.artifact_id = "artifact-preview-execution-alignment";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    const auto plan_id = prepare_result.Value().plan_id;

    const auto& prepared_plan = use_case.plans_.at(plan_id);
    EXPECT_FALSE(prepared_plan.preview_authority_shared_with_execution);
    EXPECT_FALSE(prepared_plan.execution_authority_shared_with_execution);
    EXPECT_FALSE(prepared_plan.execution_binding_ready);
    EXPECT_FALSE(static_cast<bool>(prepared_plan.execution_launch.execution_package));

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = plan_id;
    snapshot_request.max_polyline_points = 128;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();
    const auto& resolved_plan = use_case.plans_.at(plan_id);
    EXPECT_TRUE(resolved_plan.preview_authority_shared_with_execution);
    EXPECT_TRUE(resolved_plan.preview_binding_ready);
    EXPECT_TRUE(resolved_plan.execution_authority_shared_with_execution);
    EXPECT_TRUE(resolved_plan.execution_binding_ready);
    EXPECT_TRUE(static_cast<bool>(resolved_plan.execution_launch.execution_package));
    EXPECT_FALSE(resolved_plan.execution_trajectory_points.empty());
    EXPECT_EQ(
        resolved_plan.response.point_count,
        static_cast<std::uint32_t>(resolved_plan.execution_trajectory_points.size()));
    EXPECT_EQ(resolved_plan.preview_snapshot_hash, prepare_result.Value().plan_fingerprint);
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

TEST(DispensingWorkflowUseCaseTest, StartJobAndExecutionResumeUseSameInterlockPreconditions) {
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

    RuntimeJobStatusResponse runtime_status;
    runtime_status.job_id = "job-1";
    runtime_status.plan_id = "plan-1";
    runtime_status.plan_fingerprint = "fp-plan-1";
    runtime_status.state = "paused";
    runtime_status.target_count = 1;
    execution_use_case->SeedJobStateForTesting(runtime_status, true);
    execution_use_case->SetActiveJobForTesting("job-1");

    auto resume_result = execution_use_case->ResumeJob("job-1");
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
    plan_record.preview_binding_ready = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    SeedAuthorityMetadata(plan_record, "layout-plan-confirmed");
    plan_record.execution_trajectory_points.emplace_back(0.0f, 0.0f, 0.0f);
    plan_record.execution_trajectory_points.emplace_back(10.0f, 0.0f, 0.0f);
    plan_record.execution_assembly.motion_trajectory_points.emplace_back(0.0f, 0.0f, 0.0f);
    plan_record.execution_assembly.motion_trajectory_points.emplace_back(10.0f, 0.0f, 0.0f);
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
    ASSERT_EQ(snapshot.glue_reveal_lengths_mm.size(), 2U);
    EXPECT_FLOAT_EQ(snapshot.glue_reveal_lengths_mm[0], 0.0f);
    EXPECT_FLOAT_EQ(snapshot.glue_reveal_lengths_mm[1], 10.0f);
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

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotCarriesPreviewDiagnosticCode) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-fragmented",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    plan_record.preview_validation_classification = "pass_with_exception";
    plan_record.preview_exception_reason = "span spacing outside configured window but accepted as explicit exception";
    plan_record.preview_diagnostic_code = "process_path_fragmentation";
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-fragmented";
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_EQ(result.Value().preview_diagnostic_code, "process_path_fragmentation");
    EXPECT_EQ(result.Value().preview_validation_classification, "pass_with_exception");
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsFailPreviewUsingFailureReason) {
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
    EXPECT_LE(snapshot.motion_preview_polyline.size(), 6U);
    EXPECT_TRUE(SnapshotContainsPoint(snapshot, 9.0f, 0.0f, 1e-4f));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotSamplesGluePointsWithoutChangingAuthorityCount) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    std::vector<Point2D> raw_points;
    for (int i = 0; i < 12; ++i) {
        raw_points.emplace_back(static_cast<float>(i), 0.0f);
    }

    auto plan_record = BuildPreviewPlanRecord("plan-glue-sampled", raw_points);
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-glue-sampled";
    request.max_polyline_points = 32;
    request.max_glue_points = 3;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    EXPECT_EQ(result.Value().glue_point_count, 12U);
    EXPECT_EQ(result.Value().point_count, 12U);
    EXPECT_EQ(result.Value().glue_points.size(), 3U);
    ASSERT_EQ(result.Value().glue_reveal_lengths_mm.size(), 3U);
    EXPECT_FLOAT_EQ(result.Value().glue_reveal_lengths_mm[0], 0.0f);
    EXPECT_FLOAT_EQ(result.Value().glue_reveal_lengths_mm[1], 6.0f);
    EXPECT_FLOAT_EQ(result.Value().glue_reveal_lengths_mm[2], 11.0f);
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotUsesExecutionTrajectoryForNestedMotionPreview) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-motion-preview",
        {
            Point2D(0.0f, 0.0f),
            Point2D(100.0f, 0.0f),
            Point2D(100.0f, 100.0f),
            Point2D(0.0f, 100.0f),
            Point2D(0.0f, 0.0f),
            Point2D(100.0f, 100.0f),
        });
    plan_record.execution_trajectory_points = {
        Siligen::TrajectoryPoint(0.0f, 100.0f, 0.0f),
        Siligen::TrajectoryPoint(100.0f, 0.0f, 0.0f),
        Siligen::TrajectoryPoint(0.0f, 0.0f, 0.0f),
    };
    plan_record.response.point_count = static_cast<std::uint32_t>(plan_record.execution_trajectory_points.size());
    plan_record.execution_assembly.motion_trajectory_points = {
        Siligen::TrajectoryPoint(0.0f, 0.0f, 0.0f),
        Siligen::TrajectoryPoint(100.0f, 0.0f, 0.0f),
        Siligen::TrajectoryPoint(100.0f, 100.0f, 0.0f),
        Siligen::TrajectoryPoint(0.0f, 100.0f, 0.0f),
        Siligen::TrajectoryPoint(0.0f, 0.0f, 0.0f),
        Siligen::TrajectoryPoint(100.0f, 100.0f, 0.0f),
    };
    plan_record.execution_assembly.export_request.process_path.segments = {
        BuildLineProcessSegment(Point2D(0.0f, 0.0f), Point2D(100.0f, 0.0f)),
        BuildLineProcessSegment(Point2D(100.0f, 0.0f), Point2D(100.0f, 100.0f)),
        BuildLineProcessSegment(Point2D(100.0f, 100.0f), Point2D(0.0f, 100.0f)),
        BuildLineProcessSegment(Point2D(0.0f, 100.0f), Point2D(0.0f, 0.0f)),
        BuildLineProcessSegment(Point2D(0.0f, 0.0f), Point2D(100.0f, 100.0f)),
    };
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-motion-preview";
    request.max_polyline_points = 64;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_EQ(snapshot.motion_preview_source, "execution_trajectory_snapshot");
    EXPECT_EQ(snapshot.motion_preview_kind, "polyline");
    EXPECT_EQ(snapshot.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving");
    EXPECT_EQ(snapshot.motion_preview_source_point_count, 6U);
    EXPECT_FALSE(snapshot.motion_preview_is_sampled);
    EXPECT_EQ(snapshot.motion_preview_point_count, snapshot.motion_preview_source_point_count);
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 100.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 100.0f, 100.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 100.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 0.0f, 1e-4f));
    EXPECT_FALSE(MotionPreviewHasUnexpectedSegmentAngle(snapshot, {0.0, 90.0, 180.0, -90.0, 45.0}));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotClampsLongExecutionTrajectoryMotionPreviewPreservingCorners) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-motion-preview-clamp",
        {
            Point2D(0.0f, 0.0f),
            Point2D(100.0f, 0.0f),
            Point2D(100.0f, 100.0f),
            Point2D(0.0f, 0.0f),
        });
    plan_record.execution_assembly.motion_trajectory_points.clear();
    for (int i = 0; i <= 2000; ++i) {
        plan_record.execution_assembly.motion_trajectory_points.emplace_back(i * 0.05f, 0.0f, 0.0f);
    }
    for (int i = 1; i <= 2000; ++i) {
        plan_record.execution_assembly.motion_trajectory_points.emplace_back(100.0f, i * 0.05f, 0.0f);
    }
    for (int i = 1; i <= 2000; ++i) {
        const auto value = 100.0f - i * 0.05f;
        plan_record.execution_assembly.motion_trajectory_points.emplace_back(value, value, 0.0f);
    }
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-motion-preview-clamp";
    request.max_polyline_points = 64;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_EQ(snapshot.motion_preview_source, "execution_trajectory_snapshot");
    EXPECT_EQ(snapshot.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving_clamp");
    EXPECT_TRUE(snapshot.motion_preview_is_sampled);
    EXPECT_GT(snapshot.motion_preview_source_point_count, snapshot.motion_preview_point_count);
    EXPECT_LE(snapshot.motion_preview_point_count, 64U);
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 100.0f, 0.0f, 1e-3f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 100.0f, 100.0f, 1e-3f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 0.0f, 1e-3f));
    EXPECT_FALSE(MotionPreviewHasUnexpectedSegmentAngle(snapshot, {0.0, 90.0, -135.0}));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotUsesProcessPathWhenMotionTrajectorySnapshotMissing) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-motion-preview-process-fallback",
        {
            Point2D(0.0f, 0.0f),
            Point2D(50.0f, 0.0f),
        });
    plan_record.execution_trajectory_points = {
        Siligen::TrajectoryPoint(0.0f, 0.0f, 0.0f),
        Siligen::TrajectoryPoint(50.0f, 0.0f, 0.0f),
    };
    plan_record.execution_assembly.motion_trajectory_points.clear();
    plan_record.execution_assembly.export_request.process_path.segments = {
        BuildLineProcessSegment(Point2D(0.0f, 0.0f), Point2D(0.0f, 25.0f)),
        BuildLineProcessSegment(Point2D(0.0f, 25.0f), Point2D(25.0f, 25.0f)),
    };
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-motion-preview-process-fallback";
    request.max_polyline_points = 64;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsSuccess());
    const auto& snapshot = result.Value();
    EXPECT_EQ(snapshot.motion_preview_source, "process_path_snapshot");
    EXPECT_EQ(snapshot.motion_preview_kind, "polyline");
    EXPECT_EQ(snapshot.motion_preview_sampling_strategy, "process_path_geometry_preserving");
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 25.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 25.0f, 25.0f, 1e-4f));
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotFailsWhenExportRequestIsReleasedAndNoExecutionTruthRemains) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    SeedPlan(use_case, "plan-authority-process-path-fallback");
    auto& plan_record = use_case.plans_.at("plan-authority-process-path-fallback");
    plan_record.execution_launch.authority_preview.success = true;
    plan_record.execution_launch.authority_preview.authority_process_path.segments = {
        BuildLineProcessSegment(Point2D(0.0f, 0.0f), Point2D(100.0f, 0.0f)),
        BuildLineProcessSegment(Point2D(100.0f, 0.0f), Point2D(100.0f, 100.0f)),
    };
    plan_record.execution_assembly.export_request = {};

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-authority-process-path-fallback";
    request.max_polyline_points = 64;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "motion trajectory snapshot unavailable for preview");
}

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotFailsWhenMotionTrajectorySnapshotMissingAndOnlyExecutionPolylineExists) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-motion-preview-legacy-fallback",
        {
            Point2D(0.0f, 0.0f),
            Point2D(9.0f, 0.0f),
            Point2D(9.0f, 9.0f),
        });
    plan_record.execution_assembly.motion_trajectory_points.clear();
    plan_record.execution_assembly.export_request.process_path.segments.clear();
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-motion-preview-legacy-fallback";
    request.max_polyline_points = 16;
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "motion trajectory snapshot unavailable for preview");
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
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
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

TEST(DispensingWorkflowUseCaseTest, StartJobPreservesAuthorityFingerprintAndGluePoints) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
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
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-consistency";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    const auto plan_id = prepare_result.Value().plan_id;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = plan_id;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest confirm_request;
    confirm_request.plan_id = plan_id;
    confirm_request.snapshot_hash = snapshot_result.Value().snapshot_hash;
    const auto confirm_result = use_case.ConfirmPreview(confirm_request);
    ASSERT_TRUE(confirm_result.IsSuccess()) << confirm_result.GetError().ToString();

    const auto before_glue_fingerprint = GluePointsFingerprint(use_case.plans_.at(plan_id).glue_points);
    const auto before_layout_id = use_case.plans_.at(plan_id).authority_trigger_layout.layout_id;

    Siligen::Application::UseCases::Dispensing::StartJobRequest start_request;
    start_request.plan_id = plan_id;
    start_request.plan_fingerprint = prepare_result.Value().plan_fingerprint;
    start_request.target_count = 1;
    const auto start_result = use_case.StartJob(start_request);

    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().ToString();
    const auto& response = start_result.Value();
    EXPECT_EQ(response.plan_id, plan_id);
    EXPECT_EQ(response.plan_fingerprint, prepare_result.Value().plan_fingerprint);
    EXPECT_EQ(snapshot_result.Value().snapshot_hash, prepare_result.Value().plan_fingerprint);
    EXPECT_EQ(use_case.plans_.at(plan_id).preview_snapshot_hash, response.plan_fingerprint);
    EXPECT_EQ(use_case.plans_.at(plan_id).authority_trigger_layout.layout_id, before_layout_id);
    EXPECT_EQ(GluePointsFingerprint(use_case.plans_.at(plan_id).glue_points), before_glue_fingerprint);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanParameterChangeInvalidatesPreviouslyConfirmedPreview) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
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
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-preview-stale";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest first_prepare_request =
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto first_prepare = use_case.PreparePlan(first_prepare_request);
    ASSERT_TRUE(first_prepare.IsSuccess()) << first_prepare.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = first_prepare.Value().plan_id;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);
    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest confirm_request;
    confirm_request.plan_id = first_prepare.Value().plan_id;
    confirm_request.snapshot_hash = snapshot_result.Value().snapshot_hash;
    const auto confirm_result = use_case.ConfirmPreview(confirm_request);
    ASSERT_TRUE(confirm_result.IsSuccess()) << confirm_result.GetError().ToString();

    PreparePlanRequest second_prepare_request = first_prepare_request;
    second_prepare_request.planning_request.spacing_max_mm = 1.5f;
    const auto second_prepare = use_case.PreparePlan(second_prepare_request);
    ASSERT_TRUE(second_prepare.IsSuccess()) << second_prepare.GetError().ToString();
    EXPECT_NE(second_prepare.Value().plan_fingerprint, first_prepare.Value().plan_fingerprint);

    Siligen::Application::UseCases::Dispensing::StartJobRequest stale_start_request;
    stale_start_request.plan_id = first_prepare.Value().plan_id;
    stale_start_request.plan_fingerprint = first_prepare.Value().plan_fingerprint;
    stale_start_request.target_count = 1;
    const auto stale_start_result = use_case.StartJob(stale_start_request);

    ASSERT_TRUE(stale_start_result.IsError());
    EXPECT_EQ(stale_start_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(stale_start_result.GetError().GetMessage(), "plan is stale");
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanExecutionStrategyPresenceUpdatesPlanFingerprint) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
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
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-strategy-fingerprint";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest first_prepare_request =
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto first_prepare = use_case.PreparePlan(first_prepare_request);
    ASSERT_TRUE(first_prepare.IsSuccess()) << first_prepare.GetError().ToString();

    PreparePlanRequest second_prepare_request = first_prepare_request;
    second_prepare_request.planning_request.requested_execution_strategy =
        Siligen::Shared::Types::DispensingExecutionStrategy::FLYING_SHOT;
    const auto second_prepare = use_case.PreparePlan(second_prepare_request);
    ASSERT_TRUE(second_prepare.IsSuccess()) << second_prepare.GetError().ToString();

    EXPECT_NE(second_prepare.Value().plan_fingerprint, first_prepare.Value().plan_fingerprint);
    EXPECT_NE(second_prepare.Value().plan_id, first_prepare.Value().plan_id);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanRecipePolicyUpdatesPlanFingerprint) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto recipe_planning_policy_port = std::make_shared<FakeProductionBaselinePort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port,
        recipe_planning_policy_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-policy-fingerprint";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    const auto first_prepare = use_case.PreparePlan(
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(first_prepare.IsSuccess()) << first_prepare.GetError().ToString();

    recipe_planning_policy_port->policy.trigger_spatial_interval_mm = 7.5f;
    const auto second_prepare = use_case.PreparePlan(
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(second_prepare.IsSuccess()) << second_prepare.GetError().ToString();

    EXPECT_NE(second_prepare.Value().plan_fingerprint, first_prepare.Value().plan_fingerprint);
    EXPECT_NE(second_prepare.Value().plan_id, first_prepare.Value().plan_id);
}

TEST(DispensingWorkflowUseCaseTest, ConfirmPreviewRejectsFlyingShotWhenInMotionTimeTriggerCapabilityMissing) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-missing-time-trigger");
    auto& plan_record = use_case.plans_.at("plan-missing-time-trigger");
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;

    auto motion_device_port = std::dynamic_pointer_cast<FakeMotionDevicePort>(use_case.motion_device_port_);
    ASSERT_TRUE(static_cast<bool>(motion_device_port));
    motion_device_port->capabilities.trigger.supports_in_motion_time_trigger = false;

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest request;
    request.plan_id = "plan-missing-time-trigger";
    request.snapshot_hash = plan_record.preview_snapshot_hash;
    const auto result = use_case.ConfirmPreview(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("in-motion time trigger"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsFlyingShotWhenContinuousModeCapabilityMissing) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-missing-continuous-mode");

    auto dispenser_device_port = std::dynamic_pointer_cast<FakeDispenserDevicePort>(use_case.dispenser_device_port_);
    ASSERT_TRUE(static_cast<bool>(dispenser_device_port));
    dispenser_device_port->capability.supports_continuous_mode = false;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-missing-continuous-mode";
    request.plan_fingerprint = "fp-plan-missing-continuous-mode";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("continuous mode"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsPointFlyingShotWhenInMotionPulseCapabilityMissing) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-missing-in-motion-pulse");
    auto& plan_record = use_case.plans_.at("plan-missing-in-motion-pulse");
    ConfigurePointFlyingShotPlan(plan_record);

    auto dispenser_device_port = std::dynamic_pointer_cast<FakeDispenserDevicePort>(use_case.dispenser_device_port_);
    ASSERT_TRUE(static_cast<bool>(dispenser_device_port));
    dispenser_device_port->capability.supports_in_motion_pulse_shot = false;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-missing-in-motion-pulse";
    request.plan_fingerprint = "fp-plan-missing-in-motion-pulse";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("in-motion pulse shot"), std::string::npos);
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
    plan_record.execution_authority_shared_with_execution = false;
    plan_record.execution_assembly.preview_authority_shared_with_execution = false;

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
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-binding-mismatch");
    auto& plan_record = use_case.plans_.at("plan-binding-mismatch");
    plan_record.execution_binding_ready = false;
    plan_record.execution_assembly.execution_binding_ready = false;
    plan_record.preview_failure_reason = "authority trigger binding unavailable";

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-binding-mismatch";
    request.plan_fingerprint = "fp-plan-binding-mismatch";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("binding"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, OnRuntimeJobTerminalClearsConfirmedPreviewState) {
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
    plan_record.execution_authority_shared_with_execution = true;
    plan_record.execution_binding_ready = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.preview_snapshot_hash = "fp-plan-finish";
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.latest = true;
    plan_record.runtime_job_id = "job-finish";
    plan_record.execution_launch.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            BuildMinimalExecutionPackage());
    plan_record.execution_trajectory_points.emplace_back(0.0f, 0.0f, 0.0f);
    plan_record.execution_trajectory_points.emplace_back(10.0f, 0.0f, 0.0f);
    plan_record.glue_points.emplace_back(0.0f, 0.0f);
    plan_record.glue_points.emplace_back(10.0f, 0.0f);
    SeedAuthorityMetadata(plan_record, "layout-plan-finish");
    plan_record.execution_assembly.success = true;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.execution_assembly.preview_authority_shared_with_execution = true;
    plan_record.execution_assembly.execution_binding_ready = true;
    plan_record.execution_assembly.execution_package = plan_record.execution_launch.execution_package;
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    const std::size_t retained_glue_points = plan_record.glue_points.size();
    const std::size_t retained_preview_points = plan_record.execution_trajectory_points.size();
    use_case.plans_[plan_record.response.plan_id] = plan_record;
    use_case.execution_assembly_cache_[plan_record.response.plan_fingerprint] =
        DispensingWorkflowUseCase::ExecutionAssemblyCacheEntry{plan_record.execution_assembly};
    use_case.job_plan_index_["job-finish"] = "plan-finish";

    use_case.OnRuntimeJobTerminal("job-finish");

    ASSERT_TRUE(use_case.plans_.find("plan-finish") != use_case.plans_.end());
    EXPECT_EQ(
        use_case.plans_.at("plan-finish").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY);
    EXPECT_TRUE(use_case.plans_.at("plan-finish").confirmed_at.empty());
    EXPECT_TRUE(use_case.plans_.at("plan-finish").runtime_job_id.empty());
    EXPECT_FALSE(use_case.plans_.at("plan-finish").execution_launch.execution_package);
    EXPECT_FALSE(use_case.plans_.at("plan-finish").execution_assembly.execution_package);
    EXPECT_FALSE(use_case.plans_.at("plan-finish").preview_authority_shared_with_execution);
    EXPECT_FALSE(use_case.plans_.at("plan-finish").execution_authority_shared_with_execution);
    EXPECT_FALSE(use_case.plans_.at("plan-finish").execution_binding_ready);
    EXPECT_EQ(use_case.plans_.at("plan-finish").glue_points.size(), retained_glue_points);
    EXPECT_EQ(use_case.plans_.at("plan-finish").execution_trajectory_points.size(), retained_preview_points);
    EXPECT_TRUE(use_case.execution_assembly_cache_.find("fp-plan-finish") == use_case.execution_assembly_cache_.end());
    EXPECT_TRUE(use_case.job_plan_index_.find("job-finish") == use_case.job_plan_index_.end());
}

TEST(DispensingWorkflowUseCaseTest, OnRuntimeJobTerminalSkipsExecutionReleaseForMismatchedRuntimeJobId) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);

    SeedPlan(use_case, "plan-mismatch");
    auto& plan_record = use_case.plans_.at("plan-mismatch");
    plan_record.runtime_job_id = "job-real";
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    use_case.execution_assembly_cache_[plan_record.response.plan_fingerprint] =
        DispensingWorkflowUseCase::ExecutionAssemblyCacheEntry{plan_record.execution_assembly};
    use_case.job_plan_index_["job-other"] = "plan-mismatch";

    use_case.OnRuntimeJobTerminal("job-other");

    ASSERT_TRUE(use_case.plans_.find("plan-mismatch") != use_case.plans_.end());
    EXPECT_EQ(use_case.plans_.at("plan-mismatch").runtime_job_id, "job-real");
    EXPECT_TRUE(static_cast<bool>(use_case.plans_.at("plan-mismatch").execution_launch.execution_package));
    EXPECT_TRUE(static_cast<bool>(use_case.plans_.at("plan-mismatch").execution_assembly.execution_package));
    EXPECT_EQ(
        use_case.plans_.at("plan-mismatch").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED);
    EXPECT_FALSE(use_case.plans_.at("plan-mismatch").confirmed_at.empty());
    EXPECT_TRUE(use_case.execution_assembly_cache_.find("fp-plan-mismatch") != use_case.execution_assembly_cache_.end());
    EXPECT_TRUE(use_case.job_plan_index_.find("job-other") == use_case.job_plan_index_.end());
}

TEST(DispensingWorkflowUseCaseTest, ReleaseConfirmedPreviewDropsRetainedExecutionStateAndAllowsReassembly) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCaseWithPlanningAndExecution(
        planning_use_case,
        execution_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-release-rebuild";
    artifact_record.upload_response.filepath = temp_pb_file.string();
    artifact_record.upload_response.success = true;
    SeedProductionReadyImportDiagnostics(artifact_record);
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    const auto plan_id = prepare_result.Value().plan_id;
    const auto plan_fingerprint = prepare_result.Value().plan_fingerprint;

    const auto first_probe = use_case.EnsureExecutionAssemblyReadyForTesting(plan_id);
    ASSERT_TRUE(first_probe.IsSuccess()) << first_probe.GetError().ToString();
    EXPECT_TRUE(first_probe.Value().has_execution_launch_package);
    EXPECT_TRUE(first_probe.Value().has_execution_assembly_package);
    EXPECT_TRUE(first_probe.Value().execution_cache_contains_plan);
    EXPECT_GT(first_probe.Value().execution_launch_interpolation_segment_count, 0U);
    EXPECT_GT(first_probe.Value().execution_launch_interpolation_point_count, 0U);
    EXPECT_GT(first_probe.Value().execution_launch_motion_point_count, 0U);

    auto& plan_record = use_case.plans_.at(plan_id);
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED;
    plan_record.confirmed_at = "2026-03-22T00:00:00Z";
    plan_record.runtime_job_id = "job-release";
    use_case.job_plan_index_["job-release"] = plan_id;

    const std::size_t retained_preview_points = plan_record.execution_trajectory_points.size();
    const std::size_t retained_glue_points = plan_record.glue_points.size();
    ASSERT_GT(retained_preview_points, 0U);
    ASSERT_GT(retained_glue_points, 0U);

    RuntimeJobStatusResponse runtime_status;
    runtime_status.job_id = "job-release";
    runtime_status.plan_id = plan_id;
    runtime_status.plan_fingerprint = plan_fingerprint;
    runtime_status.state = "running";
    runtime_status.target_count = 1;
    execution_use_case->SeedJobStateForTesting(runtime_status);
    execution_use_case->SetActiveJobForTesting("job-release");

    use_case.OnRuntimeJobTerminal("job-release", plan_id);

    ASSERT_TRUE(use_case.plans_.find(plan_id) != use_case.plans_.end());
    EXPECT_EQ(
        use_case.plans_.at(plan_id).preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY);
    EXPECT_TRUE(use_case.plans_.at(plan_id).confirmed_at.empty());
    EXPECT_TRUE(use_case.plans_.at(plan_id).runtime_job_id.empty());
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_launch.execution_package);
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_assembly.execution_package);
    EXPECT_FALSE(use_case.plans_.at(plan_id).preview_authority_shared_with_execution);
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_authority_shared_with_execution);
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_binding_ready);
    EXPECT_EQ(use_case.plans_.at(plan_id).execution_trajectory_points.size(), retained_preview_points);
    EXPECT_EQ(use_case.plans_.at(plan_id).glue_points.size(), retained_glue_points);
    EXPECT_TRUE(use_case.execution_assembly_cache_.find(plan_fingerprint) == use_case.execution_assembly_cache_.end());

    const auto second_probe = use_case.EnsureExecutionAssemblyReadyForTesting(plan_id);
    ASSERT_TRUE(second_probe.IsSuccess()) << second_probe.GetError().ToString();
    EXPECT_FALSE(second_probe.Value().cache_hit);
    EXPECT_TRUE(second_probe.Value().has_execution_launch_package);
    EXPECT_TRUE(second_probe.Value().has_execution_assembly_package);
    EXPECT_TRUE(second_probe.Value().execution_cache_contains_plan);
    EXPECT_GT(second_probe.Value().execution_launch_interpolation_segment_count, 0U);
    EXPECT_GT(second_probe.Value().execution_launch_interpolation_point_count, 0U);
    EXPECT_GT(second_probe.Value().execution_launch_motion_point_count, 0U);
    EXPECT_GT(second_probe.Value().execution_assembly_interpolation_segment_count, 0U);
    EXPECT_GT(second_probe.Value().execution_assembly_interpolation_point_count, 0U);
    EXPECT_GT(second_probe.Value().execution_assembly_motion_point_count, 0U);
}

