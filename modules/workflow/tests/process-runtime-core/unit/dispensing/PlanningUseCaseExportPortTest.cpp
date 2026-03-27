#include "application/usecases/dispensing/PlanningUseCase.h"

#include "application/services/dxf/DxfPbPreparationService.h"
#include "domain/trajectory/ports/IPathSourcePort.h"
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
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanner;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Trajectory::Ports::IPathSourcePort;
using Siligen::Domain::Trajectory::Ports::PathSourceResult;
using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;
using Siligen::Shared::Types::Point2D;
using Siligen::Workflow::Contracts::WorkflowPlanningTriggerRequest;
template <typename T>
using ResultT = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;

class FakeConfigurationPort final : public IConfigurationPort {
public:
    ResultT<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        Siligen::Domain::Configuration::Ports::SystemConfig config;
        config.machine.max_speed = 100.0f;
        config.machine.max_acceleration = 500.0f;
        config.machine.pulse_per_mm = 200.0f;
        config.dispensing.dot_diameter_target_mm = 1.0f;
        config.dispensing.dot_edge_gap_mm = 1.0f;
        return ResultT<Siligen::Domain::Configuration::Ports::SystemConfig>::Success(config);
    }
    ResultVoid SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override { return ResultVoid::Success(); }
    ResultVoid ReloadConfiguration() override { return ResultVoid::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success({}); }
    ResultVoid SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override { return ResultVoid::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig> GetDxfPreprocessConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override { return ResultT<Siligen::Shared::Types::DiagnosticsConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::MachineConfig>::Success({}); }
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
    request.optimize_path = true;
    return request;
}

}  // namespace

TEST(PlanningUseCaseExportPortTest, ExecuteBuildsExportRequestWithoutDirectFilesystemOwnership) {
    auto temp_pb = MakeTempPbPath();
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto path_source = std::make_shared<FakePathSourcePort>();
    auto planner = std::make_shared<DispensingPlanner>(path_source);
    auto export_port = std::make_shared<FakePlanningArtifactExportPort>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    PlanningUseCase use_case(planner, config_port, pb_service, export_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_EQ(export_port->export_calls, 1);
    EXPECT_EQ(export_port->last_request.source_path, temp_pb.string());
    EXPECT_FALSE(export_port->last_request.execution_trajectory_points.empty());
    EXPECT_FALSE(export_port->last_request.glue_points.empty());

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
