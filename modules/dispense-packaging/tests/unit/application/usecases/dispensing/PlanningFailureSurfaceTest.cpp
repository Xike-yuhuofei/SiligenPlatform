#include "dispense_packaging/application/usecases/dispensing/PlanningUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "trace_diagnostics/contracts/IDiagnosticsPort.h"
#include "runtime/contracts/system/IEventPublisherPort.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Primitive.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {

using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Domain::Configuration::Ports::HomingConfig;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using Siligen::Domain::Diagnostics::Ports::DiagnosticInfo;
using Siligen::Domain::Diagnostics::Ports::DiagnosticLevel;
using Siligen::Domain::Diagnostics::Ports::HealthReport;
using Siligen::Domain::Diagnostics::Ports::HealthState;
using Siligen::Domain::Diagnostics::Ports::IDiagnosticsPort;
using Siligen::Domain::Diagnostics::Ports::PerformanceMetrics;
using Siligen::Domain::System::Ports::DomainEvent;
using Siligen::Domain::System::Ports::EventHandler;
using Siligen::Domain::System::Ports::EventType;
using Siligen::Domain::System::Ports::IEventPublisherPort;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::IPathSourcePort;
using Siligen::ProcessPath::Contracts::PathSourceResult;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::TrajectoryConfig;

template <typename T>
using ResultT = Siligen::Shared::Types::Result<T>;

std::string QuoteArg(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) {
        return value;
    }
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

class FakeConfigurationPort final : public IConfigurationPort {
public:
    FakeConfigurationPort() {
        system_config_.machine.max_speed = 100.0f;
        system_config_.machine.max_acceleration = 500.0f;
        system_config_.machine.pulse_per_mm = 200.0f;
    }

    ResultT<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return ResultT<Siligen::Domain::Configuration::Ports::SystemConfig>::Success(system_config_);
    }
    Result<void> SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override { return Result<void>::Success(); }
    Result<void> ReloadConfiguration() override { return Result<void>::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success({}); }
    Result<void> SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override { return Result<void>::Success(); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfImportConfig> GetDxfImportConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfImportConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override { return ResultT<Siligen::Shared::Types::DiagnosticsConfig>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::MachineConfig>::Success(system_config_.machine); }
    Result<void> SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig&) override { return Result<void>::Success(); }
    ResultT<HomingConfig> GetHomingConfig(int axis) const override { HomingConfig config; config.axis = axis; return ResultT<HomingConfig>::Success(config); }
    Result<void> SetHomingConfig(int, const HomingConfig&) override { return Result<void>::Success(); }
    ResultT<std::vector<HomingConfig>> GetAllHomingConfigs() const override { return ResultT<std::vector<HomingConfig>>::Success({}); }
    ResultT<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override { return ResultT<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override { return ResultT<Siligen::Shared::Types::DispenserValveConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override { return ResultT<Siligen::Shared::Types::ValveCoordinationConfig>::Success({}); }
    ResultT<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override { return ResultT<Siligen::Shared::Types::VelocityTraceConfig>::Success({}); }
    ResultT<bool> ValidateConfiguration() const override { return ResultT<bool>::Success(true); }
    ResultT<std::vector<std::string>> GetValidationErrors() const override { return ResultT<std::vector<std::string>>::Success({}); }
    Result<void> BackupConfiguration(const std::string&) override { return Result<void>::Success(); }
    Result<void> RestoreConfiguration(const std::string&) override { return Result<void>::Success(); }
    ResultT<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override { return ResultT<Siligen::Shared::Types::HardwareMode>::Success(Siligen::Shared::Types::HardwareMode::Mock); }
    ResultT<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override { return ResultT<Siligen::Shared::Types::HardwareConfiguration>::Success({}); }

private:
    Siligen::Domain::Configuration::Ports::SystemConfig system_config_{};
};

class FakePathSourcePort final : public IPathSourcePort {
public:
    ResultT<PathSourceResult> LoadFromFile(const std::string&) override {
        PathSourceResult result;
        result.primitives.push_back(Primitive::MakeLine({0.0f, 0.0f}, {10.0f, 0.0f}));
        result.metadata.push_back({});
        return ResultT<PathSourceResult>::Success(result);
    }
};

class FakeDiagnosticsPort final : public IDiagnosticsPort {
public:
    Result<HealthReport> GetHealthReport() const override { return Result<HealthReport>::Success({}); }
    Result<HealthState> GetHealthState() const override { return Result<HealthState>::Success(HealthState::HEALTHY); }
    Result<bool> IsSystemHealthy() const override { return Result<bool>::Success(true); }
    Result<std::vector<DiagnosticInfo>> GetDiagnostics(DiagnosticLevel = DiagnosticLevel::INFO) const override {
        return Result<std::vector<DiagnosticInfo>>::Success(diagnostics);
    }
    Result<void> AddDiagnostic(const DiagnosticInfo& info) override { diagnostics.push_back(info); return Result<void>::Success(); }
    Result<void> ClearDiagnostics() override { diagnostics.clear(); return Result<void>::Success(); }
    Result<PerformanceMetrics> GetPerformanceMetrics() const override { return Result<PerformanceMetrics>::Success({}); }
    Result<void> ResetPerformanceMetrics() override { return Result<void>::Success(); }
    Result<bool> IsComponentHealthy(const std::string&) const override { return Result<bool>::Success(true); }
    Result<std::vector<std::string>> GetUnhealthyComponents() const override { return Result<std::vector<std::string>>::Success({}); }

    mutable std::vector<DiagnosticInfo> diagnostics;
};

class FakeEventPublisher final : public IEventPublisherPort {
public:
    Result<void> Publish(const DomainEvent& event) override { events.push_back(event); return Result<void>::Success(); }
    Result<void> PublishAsync(const DomainEvent& event) override { return Publish(event); }
    Result<int32_t> Subscribe(EventType, EventHandler) override { return Result<int32_t>::Success(1); }
    Result<void> Unsubscribe(int32_t) override { return Result<void>::Success(); }
    Result<void> UnsubscribeAll(EventType) override { return Result<void>::Success(); }
    Result<std::vector<std::shared_ptr<const DomainEvent>>> GetEventHistory(EventType, int32_t = 100) const override {
        return Result<std::vector<std::shared_ptr<const DomainEvent>>>::Success({});
    }
    Result<void> ClearEventHistory() override { events.clear(); return Result<void>::Success(); }

    mutable std::vector<DomainEvent> events;
};

class FailingExportPort final : public IPlanningArtifactExportPort {
public:
    ResultT<PlanningArtifactExportResult> Export(const PlanningArtifactExportRequest&) override {
        return ResultT<PlanningArtifactExportResult>::Failure(
            Error(ErrorCode::INVALID_STATE, "export sink unavailable", "FailingExportPort"));
    }
};

class NegativeExportAckPort final : public IPlanningArtifactExportPort {
public:
    ResultT<PlanningArtifactExportResult> Export(const PlanningArtifactExportRequest&) override {
        PlanningArtifactExportResult result;
        result.export_requested = true;
        result.success = false;
        result.message = "artifact sink rejected payload";
        return ResultT<PlanningArtifactExportResult>::Success(result);
    }
};

std::filesystem::path DxfGeometryApplicationRoot() {
    auto cursor = std::filesystem::path(__FILE__).parent_path();
    while (!cursor.empty()) {
        const auto candidate = cursor / "modules" / "dxf-geometry" / "application" / "engineering_data";
        if (std::filesystem::exists(candidate)) {
            return cursor / "modules" / "dxf-geometry" / "application";
        }
        cursor = cursor.parent_path();
    }
    return {};
}

std::filesystem::path MakeTempPbPath() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("siligen_workflow_owner_test_" + std::to_string(suffix) + ".pb");

    const auto script_path = path.parent_path() / (path.stem().string() + "_emit_pb.py");
    std::ofstream script(script_path, std::ios::binary);
    script
        << "import json\n"
        << "import pathlib\n"
        << "import sys\n"
        << "sys.path.insert(0, r'" << DxfGeometryApplicationRoot().generic_string() << "')\n"
        << "from engineering_data.proto import dxf_primitives_pb2 as pb\n"
        << "output_path = pathlib.Path(sys.argv[1])\n"
        << "bundle = pb.PathBundle()\n"
        << "bundle.header.schema_version = 2\n"
        << "bundle.header.source_path = 'planning-failure-surface-test.dxf'\n"
        << "bundle.header.units = 'mm'\n"
        << "bundle.header.unit_scale = 1.0\n"
        << "line = bundle.primitives.add()\n"
        << "line.type = pb.PRIMITIVE_LINE\n"
        << "line.line.start.x = 0.0\n"
        << "line.line.start.y = 0.0\n"
        << "line.line.end.x = 10.0\n"
        << "line.line.end.y = 0.0\n"
        << "output_path.write_bytes(bundle.SerializeToString())\n"
        << "output_path.with_suffix('.validation.json').write_text(json.dumps({\n"
        << "  'report_id': 'report-planning-failure',\n"
        << "  'schema_version': 'DXFValidationReport.v1',\n"
        << "  'file': {'file_hash': 'sha256-planning-failure', 'source_drawing_ref': 'sha256:sha256-planning-failure'},\n"
        << "  'summary': {'gate_result': 'PASS'},\n"
        << "  'classification': 'success',\n"
        << "  'preview_ready': True,\n"
        << "  'production_ready': True,\n"
        << "  'operator_summary': 'DXF import succeeded and is ready for production.',\n"
        << "  'primary_code': '',\n"
        << "  'warning_codes': [],\n"
        << "  'error_codes': [],\n"
        << "  'resolved_units': 'mm',\n"
        << "  'resolved_unit_scale': 1.0\n"
        << "}, indent=2), encoding='utf-8')\n";
    script.close();

    const auto command = "python " + QuoteArg(script_path.string()) + " " + QuoteArg(path.string());
    EXPECT_EQ(std::system(command.c_str()), 0);

    std::error_code ec;
    std::filesystem::remove(script_path, ec);

    return path;
}

std::shared_ptr<Siligen::Application::Services::Dispensing::IWorkflowPlanningAssemblyOperations> CreatePlanningOperations() {
    return Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider{}
        .CreateOperations();
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
    request.use_interpolation_planner = true;
    return request;
}

TEST(PlanningFailureSurfaceTest, ExportFailureBecomesExplicitFailureAndWritesEvidence) {
    auto temp_pb = MakeTempPbPath();
    auto diagnostics_port = std::make_shared<FakeDiagnosticsPort>();
    auto event_port = std::make_shared<FakeEventPublisher>();
    PlanningUseCase use_case(
        std::make_shared<FakePathSourcePort>(),
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        std::make_shared<FakeConfigurationPort>(),
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(
            std::make_shared<DxfPbPreparationService>()),
        std::make_shared<FailingExportPort>(),
        diagnostics_port,
        event_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    ASSERT_FALSE(diagnostics_port->diagnostics.empty());
    EXPECT_EQ(diagnostics_port->diagnostics.back().component, "dispense_packaging.planning");
    ASSERT_FALSE(event_port->events.empty());
    EXPECT_EQ(event_port->events.back().type, EventType::WORKFLOW_EXPORT_FAILED);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

TEST(PlanningFailureSurfaceTest, ExportNegativeAckBecomesExplicitFailureAndWritesEvidence) {
    auto temp_pb = MakeTempPbPath();
    auto diagnostics_port = std::make_shared<FakeDiagnosticsPort>();
    auto event_port = std::make_shared<FakeEventPublisher>();
    PlanningUseCase use_case(
        std::make_shared<FakePathSourcePort>(),
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        std::make_shared<FakeConfigurationPort>(),
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(
            std::make_shared<DxfPbPreparationService>()),
        std::make_shared<NegativeExportAckPort>(),
        diagnostics_port,
        event_port);

    const auto result = use_case.Execute(MakePlanningRequest(temp_pb));

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    ASSERT_FALSE(diagnostics_port->diagnostics.empty());
    EXPECT_EQ(diagnostics_port->diagnostics.back().component, "dispense_packaging.planning");
    EXPECT_EQ(diagnostics_port->diagnostics.back().message, "artifact sink rejected payload");
    ASSERT_FALSE(event_port->events.empty());
    EXPECT_EQ(event_port->events.back().type, EventType::WORKFLOW_EXPORT_FAILED);

    std::error_code ec;
    std::filesystem::remove(temp_pb, ec);
}

}  // namespace
