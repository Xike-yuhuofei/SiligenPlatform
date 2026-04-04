#include "application/usecases/dispensing/PlanningUseCase.h"

#include "application/services/dispensing/AuthorityPreviewAssemblyService.h"
#include "application/services/dispensing/ExecutionAssemblyService.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/services/dxf/DxfPbPreparationService.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Primitive.h"
#include "workflow/contracts/WorkflowContracts.h"

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
using Siligen::Application::Services::Dispensing::PlanningArtifactExportRequest;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Trajectory::Ports::IPathSourcePort;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::Shared::Types::Point2D;
using Siligen::Workflow::Contracts::WorkflowPlanningTriggerRequest;
template <typename T>
using ResultT = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;

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
    ResultT<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig> GetDxfPreprocessConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success({}); }
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

std::filesystem::path MakeTempPbPath() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_planning_export_" + std::to_string(suffix) + ".pb");
    std::ofstream output(path, std::ios::binary);
    output << "fake-pb";
    output.close();
    return path;
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

}  // namespace

TEST(PlanningUseCaseExportPortTest, ExecuteBuildsExportRequestWithoutDirectFilesystemOwnership) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
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

TEST(PlanningUseCaseExportPortTest, AssembleExecutionDropsExportRequestAfterExportCompletes) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
        export_port);

    const auto request = MakePlanningRequest(temp_pb);
    const auto authority_result = use_case.PrepareAuthorityPreview(request);
    ASSERT_TRUE(authority_result.IsSuccess()) << authority_result.GetError().ToString();

    const auto assembly_result = use_case.AssembleExecutionFromAuthority(request, authority_result.Value());

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
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
        export_port);
    const auto single = single_use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(single.IsSuccess()) << single.GetError().ToString();
    const auto single_glue_points = export_port->last_request.glue_points;

    PlanningUseCase subdivided_use_case(
        std::make_shared<EquivalentSubdivisionPathSourcePort>(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
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

TEST(PlanningUseCaseExportPortTest, ExecuteIgnoresPointNoiseForPreviewGlueSemantics) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase baseline_use_case(
        std::make_shared<FakePathSourcePort>(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
        export_port);
    const auto baseline = baseline_use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(baseline.IsSuccess()) << baseline.GetError().ToString();
    const auto baseline_glue_points = export_port->last_request.glue_points;

    PlanningUseCase noisy_use_case(
        std::make_shared<PointNoisePathSourcePort>(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
        export_port);
    const auto noisy = noisy_use_case.Execute(MakePlanningRequest(temp_pb));
    ASSERT_TRUE(noisy.IsSuccess()) << noisy.GetError().ToString();
    const auto& noisy_glue_points = export_port->last_request.glue_points;

    ASSERT_EQ(baseline_glue_points.size(), noisy_glue_points.size());
    EXPECT_EQ(CountPointsNear(noisy_glue_points, Point2D{5.0f, 5.0f}, 1e-4f), 0U);
    for (std::size_t index = 0; index < baseline_glue_points.size(); ++index) {
        EXPECT_NEAR(baseline_glue_points[index].x, noisy_glue_points[index].x, 1e-4f);
        EXPECT_NEAR(baseline_glue_points[index].y, noisy_glue_points[index].y, 1e-4f);
    }

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningUseCaseExportPortTest, WorkflowContractsRemainConstructibleForPlanningTrigger) {
    WorkflowPlanningTriggerRequest request;
    request.workflow_run_id = "run-1";
    request.source_artifact = "WorkflowRun";
    request.source_path = "artifact.pb";
    request.optimize_path = true;

    EXPECT_EQ(request.workflow_run_id, "run-1");
    EXPECT_EQ(request.source_artifact, "WorkflowRun");
    EXPECT_TRUE(request.optimize_path);
}

TEST(PlanningUseCaseExportPortTest, ExecuteKeepsSharedVertexExportStableAcrossRepeatedPlanning) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<SharedVertexPathSourcePort>();
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(
        path_source,
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
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

TEST(PlanningUseCaseExportPortTest, ExecuteAutoFitsOutOfStrokeGeometryIntoMachineSoftLimits) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    config_port->SetMachineSoftLimits(0.0f, 480.0f, 0.0f, 480.0f);
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();

    PlanningUseCase use_case(
        std::make_shared<FittableOutOfStrokePathSourcePort>(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
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
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        config_port,
        pb_service,
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
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        left_config,
        pb_service,
        nullptr);
    PlanningUseCase right_use_case(
        std::make_shared<FakePathSourcePort>(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningPathPreparationPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningMotionPlanPort(),
        Siligen::Application::UseCases::Dispensing::CreateDefaultPlanningAssemblyPort(),
        right_config,
        pb_service,
        nullptr);

    EXPECT_NE(left_use_case.BuildAuthorityCacheKey(request), right_use_case.BuildAuthorityCacheKey(request));

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}





