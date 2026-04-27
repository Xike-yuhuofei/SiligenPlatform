#include "dispense_packaging/application/usecases/dispensing/PlanningUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/PathGenerationResult.h"
#include "process_path/contracts/Primitive.h"
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::IPathSourcePort;
using Siligen::ProcessPath::Contracts::PathGenerationResult;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::PathSourceResult;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::DispensingExecutionGeometryKind;
using Siligen::Shared::Types::DispensingExecutionStrategy;
using Siligen::Shared::Types::PointFlyingCarrierDirectionMode;
using Siligen::Shared::Types::PointFlyingCarrierPolicy;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::Shared::Types::Point2D;
template <typename T>
using ResultT = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;

Siligen::ProcessPath::Contracts::ProcessSegment MakeLineProcessSegment(
    const Point2D& start,
    const Point2D& end) {
    Siligen::ProcessPath::Contracts::ProcessSegment segment;
    segment.dispense_on = true;
    segment.geometry.type = Siligen::ProcessPath::Contracts::SegmentType::Line;
    segment.geometry.line.start = start;
    segment.geometry.line.end = end;
    segment.geometry.length = start.DistanceTo(end);
    return segment;
}

class FakeConfigurationPort final : public IConfigurationPort {
public:
    FakeConfigurationPort() {
        system_config_.machine.max_speed = 100.0f;
        system_config_.machine.max_acceleration = 500.0f;
        system_config_.machine.pulse_per_mm = 200.0f;
        system_config_.machine.soft_limits.x_min = 0.0f;
        system_config_.machine.soft_limits.x_max = 480.0f;
        system_config_.machine.soft_limits.y_min = 0.0f;
        system_config_.machine.soft_limits.y_max = 480.0f;
        system_config_.dispensing.dot_diameter_target_mm = 1.0f;
        system_config_.dispensing.dot_edge_gap_mm = 1.0f;

        machine_config_ = system_config_.machine;
    }

    void SetMachineSoftLimits(float x_min, float x_max, float y_min, float y_max) {
        machine_config_.soft_limits.x_min = x_min;
        machine_config_.soft_limits.x_max = x_max;
        machine_config_.soft_limits.y_min = y_min;
        machine_config_.soft_limits.y_max = y_max;
        system_config_.machine.soft_limits = machine_config_.soft_limits;
    }

    ResultT<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return ResultT<Siligen::Domain::Configuration::Ports::SystemConfig>::Success(system_config_);
    }
    ResultVoid SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override { return ResultVoid::Success(); }
    ResultVoid ReloadConfiguration() override { return ResultVoid::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success({}); }
    ResultVoid SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override { return ResultVoid::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfImportConfig> GetDxfImportConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfImportConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override { return ResultT<Siligen::Shared::Types::DiagnosticsConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return ResultT<Siligen::Domain::Configuration::Ports::MachineConfig>::Success(machine_config_);
    }
    ResultVoid SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig&) override { return ResultVoid::Success(); }
    ResultT<HomingConfig> GetHomingConfig(int axis) const override {
        HomingConfig config;
        config.axis = axis;
        config.rapid_velocity = 25.0f;
        config.locate_velocity = 10.0f;
        config.acceleration = 100.0f;
        config.deceleration = 100.0f;
        return ResultT<HomingConfig>::Success(config);
    }
    ResultVoid SetHomingConfig(int, const HomingConfig&) override { return ResultVoid::Success(); }
    ResultT<std::vector<HomingConfig>> GetAllHomingConfigs() const override { return ResultT<std::vector<HomingConfig>>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override { return ResultT<Siligen::Shared::Types::DispenserValveConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override { return ResultT<Siligen::Shared::Types::ValveCoordinationConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override { return ResultT<Siligen::Shared::Types::VelocityTraceConfig>::Success({}); }
    ResultT<bool> ValidateConfiguration() const override { return ResultT<bool>::Success(true); }
    ResultT<std::vector<std::string>> GetValidationErrors() const override { return ResultT<std::vector<std::string>>::Success({}); }
    ResultVoid BackupConfiguration(const std::string&) override { return ResultVoid::Success(); }
    ResultVoid RestoreConfiguration(const std::string&) override { return ResultVoid::Success(); }
    ResultT<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override { return ResultT<Siligen::Shared::Types::HardwareMode>::Success(Siligen::Shared::Types::HardwareMode::Mock); }
    ResultT<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override { return ResultT<Siligen::Shared::Types::HardwareConfiguration>::Success({}); }

private:
    Siligen::Domain::Configuration::Ports::SystemConfig system_config_{};
    Siligen::Domain::Configuration::Ports::MachineConfig machine_config_{};
};

class FakePathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}));
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class SharedVertexPathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{0.0f, 0.0f}, Point2D{9.0f, 0.0f}));
        result.primitives.push_back(Primitive::MakeLine(Point2D{9.0f, 0.0f}, Point2D{9.0f, 9.0f}));
        result.metadata.push_back({});
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class EquivalentSubdivisionPathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{0.0f, 0.0f}, Point2D{5.0f, 0.0f}));
        result.primitives.push_back(Primitive::MakeLine(Point2D{5.0f, 0.0f}, Point2D{10.0f, 0.0f}));
        result.metadata.push_back({});
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class PointNoisePathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}));
        result.primitives.push_back(Primitive::MakePoint(Point2D{5.0f, 5.0f}));
        result.metadata.push_back({});
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class PointOnlyPathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakePoint(Point2D{5.0f, 5.0f}));
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class FittableOutOfStrokePathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{2468.813f, 1788.400f}, Point2D{2597.143f, 1788.400f}));
        result.primitives.push_back(Primitive::MakeLine(Point2D{2597.143f, 1788.400f}, Point2D{2597.143f, 1922.596f}));
        result.primitives.push_back(Primitive::MakeLine(Point2D{2597.143f, 1922.596f}, Point2D{2468.813f, 1922.596f}));
        result.primitives.push_back(Primitive::MakeLine(Point2D{2468.813f, 1922.596f}, Point2D{2468.813f, 1788.400f}));
        result.metadata.assign(result.primitives.size(), {});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class TooWidePathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string& filepath) override {
        loaded_paths.push_back(filepath);
        PathSourceResult result;
        result.success = true;
        result.primitives.push_back(Primitive::MakeLine(Point2D{0.0f, 0.0f}, Point2D{520.0f, 0.0f}));
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }

    std::vector<std::string> loaded_paths;
};

class FakePlanningArtifactExportPort final : public IPlanningArtifactExportPort {
public:
    ResultT<PlanningArtifactExportResult> Export(const PlanningArtifactExportRequest& request) override {
        ++export_calls;
        last_request = request;
        PlanningArtifactExportResult result;
        result.export_requested = true;
        result.success = true;
        return ResultT<PlanningArtifactExportResult>::Success(result);
    }

    int export_calls = 0;
    PlanningArtifactExportRequest last_request;
};

class DistinctAuthorityProcessPathPort final : public Siligen::Application::Ports::Dispensing::IProcessPathBuildPort {
public:
    Siligen::Application::Ports::Dispensing::ProcessPathBuildResult Build(
        const Siligen::Application::Ports::Dispensing::ProcessPathBuildRequest&) const override {
        PathGenerationResult result;
        result.status = PathGenerationStatus::Success;

        result.process_path.segments.push_back(MakeLineProcessSegment(Point2D{0.0f, 0.0f}, Point2D{10.0f, 0.0f}));
        result.shaped_path.segments = {
            MakeLineProcessSegment(Point2D{0.0f, 0.0f}, Point2D{4.0f, 0.0f}),
            MakeLineProcessSegment(Point2D{4.0f, 0.0f}, Point2D{10.0f, 0.0f}),
        };
        return result;
    }
};

std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> CreatePlanningOperations() {
    return Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider{}
        .CreateOperations();
}

std::filesystem::path MakeTempPbPath() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_planning_export_" + std::to_string(suffix) + ".pb");
    std::ofstream output(path, std::ios::binary);
    output << "fake-pb";
    output.close();
    return path;
}

PointFlyingCarrierPolicy BuildCanonicalPointFlyingCarrierPolicy() {
    PointFlyingCarrierPolicy policy;
    policy.direction_mode = PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    policy.trigger_spatial_interval_mm = 5.0f;
    return policy;
}

PlanningRequest MakePlanningRequest(const std::filesystem::path& pb_path) {
    PlanningRequest request;
    request.dxf_filepath = pb_path.string();
    request.trajectory_config = TrajectoryConfig();
    request.trajectory_config.max_velocity = 100.0f;
    request.trajectory_config.max_acceleration = 500.0f;
    request.trajectory_config.time_step = 0.01f;
    request.trajectory_config.trigger_pulse_us = 1000;
    request.trajectory_config.dispensing_interval = 3.0f;
    request.optimize_path = true;
    request.use_interpolation_planner = true;
    request.point_flying_carrier_policy = BuildCanonicalPointFlyingCarrierPolicy();
    return request;
}

std::size_t CountPointsNear(const std::vector<Point2D>& points, const Point2D& target, float tolerance_mm) {
    std::size_t count = 0;
    for (const auto& point : points) {
        if (point.DistanceTo(target) <= tolerance_mm) {
            ++count;
        }
    }
    return count;
}

bool AllPointsWithinBounds(
    const std::vector<Point2D>& points,
    float min_x,
    float max_x,
    float min_y,
    float max_y,
    float tolerance_mm = 1e-3f) {
    for (const auto& point : points) {
        if (point.x < min_x - tolerance_mm || point.x > max_x + tolerance_mm ||
            point.y < min_y - tolerance_mm || point.y > max_y + tolerance_mm) {
            return false;
        }
    }
    return true;
}

bool AllProcessPathSegmentEndpointsWithinBounds(
    const Siligen::ProcessPath::Contracts::ProcessPath& process_path,
    float min_x,
    float max_x,
    float min_y,
    float max_y,
    float tolerance_mm = 1e-3f) {
    for (const auto& segment : process_path.segments) {
        if (segment.geometry.type == Siligen::ProcessPath::Contracts::SegmentType::Line) {
            const auto& start = segment.geometry.line.start;
            const auto& end = segment.geometry.line.end;
            if (start.x < min_x - tolerance_mm || start.x > max_x + tolerance_mm ||
                start.y < min_y - tolerance_mm || start.y > max_y + tolerance_mm ||
                end.x < min_x - tolerance_mm || end.x > max_x + tolerance_mm ||
                end.y < min_y - tolerance_mm || end.y > max_y + tolerance_mm) {
                return false;
            }
        }
    }
    return true;
}

std::shared_ptr<Siligen::Application::Ports::Dispensing::IProcessPathBuildPort> CreateProcessPathPort() {
    return Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>());
}

std::shared_ptr<Siligen::Application::Ports::Dispensing::IMotionPlanningPort> CreateMotionPlanningPort() {
    return Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>());
}

class TestPlanningInputPreparationPort final
    : public Siligen::Application::Ports::Dispensing::IPlanningInputPreparationPort {
public:
    explicit TestPlanningInputPreparationPort(const std::shared_ptr<DxfPbPreparationService>& pb_service)
        : fallback_(Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service)) {}

    ResultT<Siligen::Application::Ports::Dispensing::PreparedPlanningInput> EnsurePreparedInput(
        const Siligen::Application::Ports::Dispensing::PlanningInputPreparationRequest& request) const override {
        const auto source_path = std::filesystem::path(request.source_path);
        if (source_path.extension() == ".pb") {
            Siligen::Application::Ports::Dispensing::PreparedPlanningInput prepared;
            prepared.prepared_path = request.source_path;
            prepared.canonical_geometry_ref = "test-canonical:" + source_path.filename().string();
            prepared.validation_report.stage_id = "S2";
            prepared.validation_report.owner_module = "M2";
            prepared.validation_report.source_ref =
                request.source_ref.empty() ? source_path.filename().string() : request.source_ref;
            prepared.validation_report.source_hash =
                request.source_hash.empty() ? "test-source-hash" : request.source_hash;
            prepared.validation_report.gate_result = "PASS";
            prepared.validation_report.result_classification = "prepared_input_ready";
            prepared.validation_report.preview_ready = true;
            prepared.validation_report.production_ready = true;
            prepared.validation_report.summary = "Test prepared input is ready.";
            prepared.validation_report.resolved_units = "mm";
            prepared.validation_report.resolved_unit_scale = 1.0;
            return ResultT<Siligen::Application::Ports::Dispensing::PreparedPlanningInput>::Success(std::move(prepared));
        }

        return fallback_->EnsurePreparedInput(request);
    }

private:
    std::shared_ptr<Siligen::Application::Ports::Dispensing::IPlanningInputPreparationPort> fallback_;
};

std::shared_ptr<Siligen::Application::Ports::Dispensing::IPlanningInputPreparationPort>
CreatePlanningInputPreparationPort(const std::shared_ptr<DxfPbPreparationService>& pb_service) {
    return std::make_shared<TestPlanningInputPreparationPort>(pb_service);
}

std::filesystem::path ResolveWorkspaceRoot() {
    auto cursor = std::filesystem::path(__FILE__).parent_path();
    while (!cursor.empty()) {
        if (std::filesystem::exists(cursor / "samples" / "dxf" / "Demo-1.dxf")) {
            return cursor;
        }
        cursor = cursor.parent_path();
    }
    return {};
}

class PreparedInputCleanupGuard {
public:
    PreparedInputCleanupGuard(
        std::shared_ptr<DxfPbPreparationService> pb_service,
        std::filesystem::path source_path,
        bool cleanup_required)
        : pb_service_(std::move(pb_service)),
          source_path_(std::move(source_path)),
          cleanup_required_(cleanup_required) {}

    ~PreparedInputCleanupGuard() {
        if (!cleanup_required_ || !pb_service_) {
            return;
        }

        pb_service_->CleanupPreparedInput(source_path_.string());
    }

private:
    std::shared_ptr<DxfPbPreparationService> pb_service_;
    std::filesystem::path source_path_;
    bool cleanup_required_ = false;
};

}  // namespace

TEST(PlanningUseCaseExportPortTest, ExecuteBuildsExportRequestWithoutDirectFilesystemOwnership) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(export_port->export_calls, 1);
    EXPECT_EQ(export_port->last_request.source_path, temp_pb.string());
    EXPECT_FALSE(export_port->last_request.execution_trajectory_points.empty());
    EXPECT_FALSE(export_port->last_request.glue_points.empty());
    EXPECT_EQ(result.Value().glue_points.size(), result.Value().authority_trigger_layout.trigger_points.size());

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, WorkflowUsesCanonicalExecutionPathAsDownstreamExportTruth) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    auto process_path_port = std::make_shared<DistinctAuthorityProcessPathPort>();

    PlanningUseCase use_case(
        path_source,
        process_path_port,
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto request = MakePlanningRequest(temp_pb);
    const auto authority_result = use_case.PrepareAuthorityPreview(request);

    ASSERT_TRUE(authority_result.IsSuccess()) << authority_result.GetError().ToString();
    ASSERT_EQ(authority_result.Value().authority_process_path.segments.size(), 2U);
    ASSERT_EQ(authority_result.Value().canonical_execution_process_path.segments.size(), 2U);
    EXPECT_FLOAT_EQ(authority_result.Value().authority_process_path.segments.front().geometry.line.end.x, 4.0f);
    EXPECT_FLOAT_EQ(authority_result.Value().authority_process_path.segments.back().geometry.line.end.x, 10.0f);
    EXPECT_FLOAT_EQ(authority_result.Value().canonical_execution_process_path.segments.front().geometry.line.end.x, 4.0f);
    EXPECT_FLOAT_EQ(authority_result.Value().canonical_execution_process_path.segments.back().geometry.line.end.x, 10.0f);

    const auto execute_result = use_case.Execute(request);

    ASSERT_TRUE(execute_result.IsSuccess()) << execute_result.GetError().ToString();
    ASSERT_EQ(export_port->last_request.process_path.segments.size(), 2U);
    EXPECT_FLOAT_EQ(export_port->last_request.process_path.segments.front().geometry.line.end.x, 4.0f);
    EXPECT_FLOAT_EQ(export_port->last_request.process_path.segments.back().geometry.line.start.x, 4.0f);
    EXPECT_FLOAT_EQ(export_port->last_request.process_path.segments.back().geometry.line.end.x, 10.0f);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, AssembleExecutionDropsExportRequestAfterExportCompletes) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto request = MakePlanningRequest(temp_pb);
    const auto authority_result = use_case.PrepareAuthorityPreview(request);
    ASSERT_TRUE(authority_result.IsSuccess()) << authority_result.GetError().ToString();

    const auto assembly_result = use_case.AssembleExecutionFromAuthority(
        request,
        authority_result.Value());

    ASSERT_TRUE(assembly_result.IsSuccess()) << assembly_result.GetError().ToString();
    ASSERT_EQ(export_port->export_calls, 1);
    EXPECT_FALSE(export_port->last_request.execution_trajectory_points.empty());
    EXPECT_FALSE(export_port->last_request.interpolation_trajectory_points.empty());
    EXPECT_FALSE(export_port->last_request.motion_trajectory_points.empty());
    EXPECT_TRUE(assembly_result.Value().export_request.execution_trajectory_points.empty());
    EXPECT_TRUE(assembly_result.Value().export_request.interpolation_trajectory_points.empty());
    EXPECT_TRUE(assembly_result.Value().export_request.motion_trajectory_points.empty());

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteExportsEquivalentGluePointsForSubdividedOpenSpan) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase single_use_case(
        std::make_shared<FakePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);
    const auto single = single_use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(single.IsSuccess()) << single.GetError().ToString();
    const auto single_glue_points = export_port->last_request.glue_points;

    PlanningUseCase subdivided_use_case(
        std::make_shared<EquivalentSubdivisionPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);
    const auto subdivided = subdivided_use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(subdivided.IsSuccess()) << subdivided.GetError().ToString();
    const auto& subdivided_glue_points = export_port->last_request.glue_points;

    ASSERT_EQ(single_glue_points.size(), subdivided_glue_points.size());
    for (std::size_t index = 0; index < single_glue_points.size(); ++index) {
        EXPECT_NEAR(single_glue_points[index].x, subdivided_glue_points[index].x, 1e-4f);
        EXPECT_NEAR(single_glue_points[index].y, subdivided_glue_points[index].y, 1e-4f);
    }

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, PrepareAuthorityPreviewIncludesPointAuthorityForMixedInput) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase noisy_use_case(
        std::make_shared<PointNoisePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);
    const auto preview = noisy_use_case.PrepareAuthorityPreview(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(preview.IsSuccess()) << preview.GetError().ToString();
    EXPECT_EQ(export_port->export_calls, 0);
    EXPECT_TRUE(preview.Value().artifacts.preview_authority_ready);
    EXPECT_TRUE(preview.Value().artifacts.preview_binding_ready);
    ASSERT_FALSE(preview.Value().artifacts.glue_points.empty());
    EXPECT_EQ(CountPointsNear(preview.Value().artifacts.glue_points, Point2D{5.0f, 5.0f}, 1e-4f), 1U);
    EXPECT_EQ(
        CountPointsNear(
            preview.Value().artifacts.glue_points,
            Point2D{10.0f / 3.0f, 0.0f},
            1e-4f),
        1U);
    EXPECT_EQ(preview.Value().artifacts.trigger_count,
              static_cast<int>(preview.Value().artifacts.glue_points.size()));

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, PrepareAuthorityPreviewSupportsPointOnlyInputForExplicitStationaryShot) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase point_only_use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);
    auto request = MakePlanningRequest(temp_pb);
    request.requested_execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;
    const auto result = point_only_use_case.PrepareAuthorityPreview(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(result.Value().artifacts.preview_authority_ready);
    EXPECT_TRUE(result.Value().artifacts.preview_binding_ready);
    ASSERT_EQ(result.Value().authority_process_path.segments.size(), 1U);
    ASSERT_EQ(result.Value().canonical_execution_process_path.segments.size(), 1U);
    EXPECT_TRUE(result.Value().authority_process_path.segments.front().geometry.is_point);
    EXPECT_TRUE(result.Value().canonical_execution_process_path.segments.front().geometry.is_point);
    ASSERT_EQ(result.Value().artifacts.glue_points.size(), 1U);
    EXPECT_EQ(CountPointsNear(result.Value().artifacts.glue_points, Point2D{5.0f, 5.0f}, 1e-4f), 1U);
    EXPECT_EQ(result.Value().artifacts.trigger_count, 1);
    EXPECT_EQ(export_port->export_calls, 0);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteKeepsSharedVertexExportStableAcrossRepeatedPlanning) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<SharedVertexPathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto first = use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(first.IsSuccess()) << first.GetError().ToString();
    const auto first_glue_points = export_port->last_request.glue_points;

    const auto second = use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(second.IsSuccess()) << second.GetError().ToString();
    const auto& second_glue_points = export_port->last_request.glue_points;

    EXPECT_EQ(first_glue_points.size(), second_glue_points.size());
    ASSERT_FALSE(second_glue_points.empty());
    EXPECT_EQ(CountPointsNear(second_glue_points, Point2D{9.0f, 0.0f}, 1e-4f), 1U);
    for (std::size_t index = 0; index < first_glue_points.size(); ++index) {
        EXPECT_NEAR(first_glue_points[index].x, second_glue_points[index].x, 1e-4f);
        EXPECT_NEAR(first_glue_points[index].y, second_glue_points[index].y, 1e-4f);
    }

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteBuildsPointFlyingShotCarrierFromExplicitPolicyForPointOnlyInput) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_TRUE(static_cast<bool>(result.Value().execution_package));
    const auto& plan = result.Value().execution_package->execution_plan;
    EXPECT_EQ(plan.geometry_kind, DispensingExecutionGeometryKind::POINT);
    EXPECT_EQ(plan.execution_strategy, DispensingExecutionStrategy::FLYING_SHOT);
    EXPECT_TRUE(plan.HasFormalTrajectory());
    ASSERT_EQ(plan.trigger_distances_mm.size(), 1U);
    EXPECT_FLOAT_EQ(plan.trigger_distances_mm.front(), 0.0f);
    ASSERT_GE(plan.motion_trajectory.points.size(), 2U);
    EXPECT_NEAR(plan.motion_trajectory.points.front().position.x, 5.0f, 1e-4f);
    EXPECT_NEAR(plan.motion_trajectory.points.front().position.y, 5.0f, 1e-4f);
    EXPECT_NEAR(plan.motion_trajectory.points.back().position.x, 8.5355f, 1e-3f);
    EXPECT_NEAR(plan.motion_trajectory.points.back().position.y, 8.5355f, 1e-3f);
    EXPECT_FLOAT_EQ(plan.total_length_mm, 5.0f);
    EXPECT_EQ(export_port->export_calls, 1);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteAllowsExplicitStationaryShotForPointOnlyInput) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    auto request = MakePlanningRequest(temp_pb);
    request.requested_execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;
    const auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_TRUE(static_cast<bool>(result.Value().execution_package));
    EXPECT_EQ(
        result.Value().execution_package->execution_plan.execution_strategy,
        DispensingExecutionStrategy::STATIONARY_SHOT);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteAllowsExplicitFlyingShotForPointOnlyInput) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    auto request = MakePlanningRequest(temp_pb);
    request.requested_execution_strategy = DispensingExecutionStrategy::FLYING_SHOT;
    const auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_TRUE(static_cast<bool>(result.Value().execution_package));
    const auto& plan = result.Value().execution_package->execution_plan;
    EXPECT_EQ(plan.geometry_kind, DispensingExecutionGeometryKind::POINT);
    EXPECT_EQ(plan.execution_strategy, DispensingExecutionStrategy::FLYING_SHOT);
    EXPECT_TRUE(plan.HasFormalTrajectory());
    ASSERT_EQ(plan.trigger_distances_mm.size(), 1U);
    EXPECT_FLOAT_EQ(plan.trigger_distances_mm.front(), 0.0f);
    EXPECT_EQ(export_port->export_calls, 1);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteRejectsPointFlyingShotWithoutCarrierPolicy) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    auto request = MakePlanningRequest(temp_pb);
    request.point_flying_carrier_policy.reset();
    const auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(result.GetError().GetMessage().find("point_flying_carrier_policy"), std::string::npos);
    EXPECT_EQ(export_port->export_calls, 0);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteRejectsPointFlyingShotWhenPlanningStartMatchesPoint) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<PointOnlyPathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    auto request = MakePlanningRequest(temp_pb);
    request.start_x = 5.0f;
    request.start_y = 5.0f;
    const auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(result.GetError().GetMessage().find("planning_start_position"), std::string::npos);
    EXPECT_EQ(export_port->export_calls, 0);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteRejectsExplicitStationaryShotForPathGeometry) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        std::make_shared<FakePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    auto request = MakePlanningRequest(temp_pb);
    request.requested_execution_strategy = DispensingExecutionStrategy::STATIONARY_SHOT;
    const auto result = use_case.Execute(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_PARAMETER);
    EXPECT_NE(
        result.GetError().GetMessage().find("stationary_shot only supports POINT geometry"),
        std::string::npos);
    EXPECT_EQ(export_port->export_calls, 0);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteAutoFitsOutOfStrokeGeometryIntoMachineSoftLimits) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    config_port->SetMachineSoftLimits(0.0f, 480.0f, 0.0f, 480.0f);
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase use_case(
        std::make_shared<FittableOutOfStrokePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_FALSE(export_port->last_request.process_path.segments.empty());
    EXPECT_TRUE(AllProcessPathSegmentEndpointsWithinBounds(
        export_port->last_request.process_path,
        0.0f,
        480.0f,
        0.0f,
        480.0f));

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, ExecuteRejectsGeometryWhoseSizeExceedsMachineSoftLimits) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    config_port->SetMachineSoftLimits(0.0f, 480.0f, 0.0f, 480.0f);
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase use_case(
        std::make_shared<TooWidePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::POSITION_OUT_OF_RANGE);
    EXPECT_NE(result.GetError().GetMessage().find("DXF超出行程范围"), std::string::npos);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, AuthorityCacheKeyIncludesMachineSoftLimits) {
    auto temp_pb = MakeTempPbPath();
    auto left_config = std::make_shared<FakeConfigurationPort>();
    auto right_config = std::make_shared<FakeConfigurationPort>();
    left_config->SetMachineSoftLimits(0.0f, 480.0f, 0.0f, 480.0f);
    right_config->SetMachineSoftLimits(0.0f, 320.0f, 0.0f, 480.0f);
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    const auto request = MakePlanningRequest(temp_pb);

    PlanningUseCase left_use_case(
        std::make_shared<FakePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        left_config,
        CreatePlanningInputPreparationPort(pb_service),
        nullptr);
    PlanningUseCase right_use_case(
        std::make_shared<FakePathSourcePort>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        right_config,
        CreatePlanningInputPreparationPort(pb_service),
        nullptr);

    EXPECT_NE(left_use_case.BuildAuthorityCacheKey(request), right_use_case.BuildAuthorityCacheKey(request));

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, Demo1DxfModuleLocalAssemblyBuildsProductionReadyFormalContract) {
    const auto workspace_root = ResolveWorkspaceRoot();
    ASSERT_FALSE(workspace_root.empty());

    const auto demo_dxf = workspace_root / "samples" / "dxf" / "Demo-1.dxf";
    ASSERT_TRUE(std::filesystem::exists(demo_dxf));

    const auto prepared_pb = demo_dxf.parent_path() / "Demo-1.pb";
    const bool prepared_pb_existed = std::filesystem::exists(prepared_pb);

    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PreparedInputCleanupGuard cleanup_guard(pb_service, demo_dxf, !prepared_pb_existed);

    PlanningUseCase use_case(
        std::make_shared<PbPathSourceAdapter>(),
        CreateProcessPathPort(),
        CreateMotionPlanningPort(),
        CreatePlanningOperations(),
        config_port,
        CreatePlanningInputPreparationPort(pb_service),
        export_port);

    const auto request = MakePlanningRequest(demo_dxf);
    const auto authority_result = use_case.PrepareAuthorityPreview(request);
    ASSERT_TRUE(authority_result.IsSuccess()) << authority_result.GetError().ToString();
    EXPECT_TRUE(authority_result.Value().artifacts.preview_authority_ready);
    EXPECT_TRUE(authority_result.Value().artifacts.preview_binding_ready);

    const auto assembly_result = use_case.AssembleExecutionFromAuthority(request, authority_result.Value());
    ASSERT_TRUE(assembly_result.IsSuccess()) << assembly_result.GetError().ToString();

    const auto& response = assembly_result.Value();
    EXPECT_TRUE(response.execution_contract_ready);
    EXPECT_FALSE(response.formal_compare_gate.HasValue());
    EXPECT_FALSE(response.authority_trigger_layout.trigger_points.empty());
    ASSERT_TRUE(static_cast<bool>(response.execution_package));
    EXPECT_FALSE(response.execution_package->execution_plan.profile_compare_program.trigger_points.empty());
    EXPECT_FALSE(response.execution_package->execution_plan.profile_compare_program.spans.empty());
}





