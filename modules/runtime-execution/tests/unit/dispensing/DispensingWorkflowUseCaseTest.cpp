#include <gtest/gtest.h>

#include <atomic>
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iomanip>
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
#include "job_ingest/application/ports/dispensing/UploadPorts.h"
#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/dispensing/IProductionBaselinePort.h"
#include "runtime_execution/contracts/dispensing/ProfileCompareExecutionCompiler.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/motion_planning/SevenSegmentSCurveProfile.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "runtime/configuration/ConfigFileAdapter.h"
#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessPath.h"
#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"
#undef private
#include "domain/dispensing/planning/domain-services/DispensingPlannerService.h"
#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"

namespace {

using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase;
using Siligen::Application::UseCases::Dispensing::JobCycleAdvanceMode;
using Siligen::Application::UseCases::Dispensing::PlanningUseCase;
using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRequest;
using Siligen::Application::UseCases::Dispensing::PreparePlanResponse;
using Siligen::Application::UseCases::Dispensing::PreparePlanRuntimeOverrides;
using Siligen::Application::UseCases::Dispensing::RuntimeJobTraceabilityResponse;
using Siligen::Application::UseCases::Dispensing::RuntimeJobStatusResponse;
using Siligen::Application::UseCases::Dispensing::StartJobResponse;
using Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter;
using Siligen::Application::Ports::Dispensing::IProductionBaselinePort;
using Siligen::Application::Ports::Dispensing::ResolvedProductionBaseline;
using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Infrastructure::Adapters::ConfigFileAdapter;
using Siligen::JobIngest::Contracts::DxfInputQuality;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::SourceDrawing;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadStoragePort;
using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
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
using Siligen::Domain::Dispensing::ValueObjects::ProfileCompareProgramSpan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
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
using Siligen::RuntimeExecution::Contracts::Dispensing::CompileProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionCompileInput;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareExecutionSchedule;
using Siligen::RuntimeExecution::Contracts::Dispensing::ProfileCompareRuntimeContract;
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
        runtime_request.cycle_advance_mode = request.cycle_advance_mode;
        runtime_request.production_baseline = request.production_baseline;
        runtime_request.input_quality = request.input_quality;
        runtime_request.execution_request.execution_package = request.execution_request.execution_package;
        runtime_request.execution_request.profile_compare_schedule = request.execution_request.profile_compare_schedule;
        runtime_request.execution_request.expected_trace = request.execution_request.expected_trace;
        runtime_request.execution_request.source_path = request.execution_request.source_path;
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

class SpyWorkflowExecutionPort final
    : public Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort {
public:
    Result<Siligen::Application::Ports::Dispensing::WorkflowJobId> StartJob(
        const Siligen::Application::Ports::Dispensing::WorkflowRuntimeStartJobRequest& request) override {
        ++start_calls;
        last_plan_id = request.plan_id;
        last_plan_fingerprint = request.plan_fingerprint;
        last_target_count = request.target_count;
        if (fail_start) {
            return Result<Siligen::Application::Ports::Dispensing::WorkflowJobId>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::INVALID_STATE,
                    failure_message,
                    "SpyWorkflowExecutionPort"));
        }
        return Result<Siligen::Application::Ports::Dispensing::WorkflowJobId>::Success(job_id);
    }

    std::size_t start_calls = 0U;
    bool fail_start = false;
    std::string failure_message = "spy execution port rejected start";
    std::string job_id = "spy-job-id";
    std::string last_plan_id;
    std::string last_plan_fingerprint;
    std::uint32_t last_target_count = 0U;
};

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

class RectDiagPathSourceStub final : public Siligen::ProcessPath::Contracts::IPathSourcePort {
   public:
    Result<Siligen::ProcessPath::Contracts::PathSourceResult> LoadFromFile(const std::string&) override {
        using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
        using Siligen::ProcessPath::Contracts::PathSourceResult;
        using Siligen::ProcessPath::Contracts::Primitive;

        PathSourceResult result;
        result.success = true;
        result.primitives = {
            Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)),
            Primitive::MakeLine(Point2D(10.0f, 0.0f), Point2D(10.0f, 10.0f)),
            Primitive::MakeLine(Point2D(10.0f, 10.0f), Point2D(0.0f, 10.0f)),
            Primitive::MakeLine(Point2D(0.0f, 10.0f), Point2D(0.0f, 0.0f)),
            Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 10.0f)),
        };
        result.metadata.resize(result.primitives.size(), PathPrimitiveMeta{});
        return Result<PathSourceResult>::Success(std::move(result));
    }
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

DxfInputQuality BuildProductionReadyInputQuality() {
    DxfInputQuality input_quality;
    input_quality.classification = "success";
    input_quality.preview_ready = true;
    input_quality.production_ready = true;
    input_quality.summary = "DXF import succeeded and is ready for production.";
    input_quality.resolved_units = "mm";
    input_quality.resolved_unit_scale = 1.0;
    return input_quality;
}

std::filesystem::path ValidationReportPathForPb(const std::filesystem::path& pb_path) {
    auto report_path = pb_path;
    report_path.replace_extension(".validation.json");
    return report_path;
}

void WriteProductionReadyValidationReport(
    const std::filesystem::path& pb_path,
    const std::string& source_hash = "test-source-hash") {
    const std::string source_drawing_ref = "sha256:" + source_hash;
    std::ofstream output(ValidationReportPathForPb(pb_path), std::ios::trunc);
    output << "{\n"
           << "  \"report_id\": \"test-validation-report\",\n"
           << "  \"schema_version\": \"DXFValidationReport.v1\",\n"
           << "  \"file\": {\n"
           << "    \"file_hash\": \"" << source_hash << "\",\n"
           << "    \"source_drawing_ref\": \"" << source_drawing_ref << "\"\n"
           << "  },\n"
           << "  \"summary\": {\"gate_result\": \"PASS\"},\n"
           << "  \"preview_ready\": true,\n"
           << "  \"production_ready\": true,\n"
           << "  \"classification\": \"success\",\n"
           << "  \"operator_summary\": \"DXF import succeeded and is ready for production.\",\n"
           << "  \"primary_code\": \"\",\n"
           << "  \"warning_codes\": [],\n"
           << "  \"error_codes\": [],\n"
           << "  \"resolved_units\": \"mm\",\n"
           << "  \"resolved_unit_scale\": 1.0\n"
           << "}\n";
}

void SeedProductionReadyInputQuality(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    plan_record.response.input_quality = BuildProductionReadyInputQuality();
}

void SeedArtifactSourceDrawing(
    DispensingWorkflowUseCase::ArtifactRecord& artifact_record,
    const std::string& filepath,
    const std::string& source_hash = "test-source-hash") {
    const std::string source_drawing_ref = "sha256:" + source_hash;
    artifact_record.response.source_drawing_ref = source_drawing_ref;
    artifact_record.response.filepath = filepath;
    artifact_record.response.source_hash = source_hash;
    artifact_record.response.validation_report.schema_version = "DXFValidationReport.v1";
    artifact_record.response.validation_report.stage_id = "S1";
    artifact_record.response.validation_report.owner_module = "M1";
    artifact_record.response.validation_report.source_ref = source_drawing_ref;
    artifact_record.response.validation_report.source_hash = source_hash;
    artifact_record.response.validation_report.gate_result = "PASS";
    artifact_record.response.validation_report.result_classification = "source_drawing_accepted";
    artifact_record.source_drawing.source_drawing_ref = source_drawing_ref;
    artifact_record.source_drawing.filepath = filepath;
    artifact_record.source_drawing.source_hash = source_hash;
    artifact_record.source_drawing.validation_report = artifact_record.response.validation_report;
}

class FakeProductionBaselinePort final : public IProductionBaselinePort {
   public:
    Result<ResolvedProductionBaseline> ResolveCurrentBaseline() const override {
        ResolvedProductionBaseline resolved;
        resolved.baseline_id = baseline_id;
        resolved.baseline_fingerprint = baseline_fingerprint;
        resolved.point_flying_carrier_policy = policy;
        return Result<ResolvedProductionBaseline>::Success(std::move(resolved));
    }

    std::string baseline_id = "test-production-baseline";
    PointFlyingCarrierPolicy policy = BuildCanonicalPointFlyingCarrierPolicy();
    std::string baseline_fingerprint = "baseline-fingerprint";
};

class FakeConfigurationPort final : public IConfigurationPort {
   public:
    Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        SyncSystemConfig();
        return Result<Siligen::Domain::Configuration::Ports::SystemConfig>::Success(system_config);
    }

    Result<void> SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override {
        return Result<void>::Success();
    }

    Result<void> ReloadConfiguration() override { return Result<void>::Success(); }

    Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success(dispensing_config);
    }

    Result<void> SetDispensingConfig(
        const Siligen::Domain::Configuration::Ports::DispensingConfig& config) override {
        dispensing_config = config;
        system_config.dispensing = config;
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Configuration::Ports::DxfImportConfig> GetDxfImportConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfImportConfig>::Success(dxf_import_config);
    }

    Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({});
    }

    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return Result<Siligen::Shared::Types::DiagnosticsConfig>::Success({});
    }

    Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::MachineConfig>::Success(machine_config);
    }

    Result<void> SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig& config) override {
        machine_config = config;
        system_config.machine = config;
        return Result<void>::Success();
    }

    Result<Siligen::Domain::Configuration::Ports::HomingConfig> GetHomingConfig(int) const override {
        return Result<Siligen::Domain::Configuration::Ports::HomingConfig>::Success({});
    }

    Result<void> SetHomingConfig(int, const Siligen::Domain::Configuration::Ports::HomingConfig&) override {
        return Result<void>::Success();
    }

    Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>> GetAllHomingConfigs() const override {
        return Result<std::vector<Siligen::Domain::Configuration::Ports::HomingConfig>>::Success({});
    }

    Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>::Success({});
    }

    Result<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override {
        return Result<Siligen::Shared::Types::DispenserValveConfig>::Success(dispenser_config);
    }

    Result<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return Result<Siligen::Shared::Types::ValveCoordinationConfig>::Success(valve_coordination_config);
    }

    Result<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return Result<Siligen::Shared::Types::VelocityTraceConfig>::Success({});
    }

    Result<bool> ValidateConfiguration() const override { return Result<bool>::Success(true); }

    Result<std::vector<std::string>> GetValidationErrors() const override {
        return Result<std::vector<std::string>>::Success({});
    }

    Result<void> BackupConfiguration(const std::string&) override { return Result<void>::Success(); }

    Result<void> RestoreConfiguration(const std::string&) override { return Result<void>::Success(); }

    Result<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override {
        return Result<Siligen::Shared::Types::HardwareMode>::Success(
            Siligen::Shared::Types::HardwareMode::Mock);
    }

    Result<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override {
        Siligen::Shared::Types::HardwareConfiguration config;
        config.pulse_per_mm = machine_config.pulse_per_mm;
        return Result<Siligen::Shared::Types::HardwareConfiguration>::Success(config);
    }

    void SyncSystemConfig() {
        system_config.machine = machine_config;
        system_config.dispensing = dispensing_config;
        system_config.dxf = dxf_config;
    }

    Siligen::Domain::Configuration::Ports::SystemConfig system_config{};
    Siligen::Domain::Configuration::Ports::MachineConfig machine_config{};
    Siligen::Domain::Configuration::Ports::DispensingConfig dispensing_config{};
    Siligen::Domain::Configuration::Ports::DxfImportConfig dxf_import_config{};
    Siligen::Domain::Configuration::Ports::DxfConfig dxf_config{};
    Siligen::Shared::Types::DispenserValveConfig dispenser_config{};
    Siligen::Shared::Types::ValveCoordinationConfig valve_coordination_config{};
};

class ScopedTempPbFile {
   public:
    ScopedTempPbFile() {
        path_ = std::filesystem::temp_directory_path() /
                ("siligen-dispensing-" + std::to_string(std::rand()) + ".pb");
        const auto fixture_path = ResolveCanonicalFixturePath();
        std::filesystem::copy_file(
            fixture_path,
            path_,
            std::filesystem::copy_options::overwrite_existing);
        WriteProductionReadyValidationReport(path_);
    }

    ~ScopedTempPbFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
        std::filesystem::remove(ValidationReportPathForPb(path_), ec);
    }

    const std::string string() const { return path_.string(); }

   private:
    static std::filesystem::path ResolveCanonicalFixturePath() {
        auto cursor = std::filesystem::path(__FILE__).parent_path();
        while (!cursor.empty()) {
            const auto candidate =
                cursor / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag" / "rect_diag.pb";
            if (std::filesystem::exists(candidate)) {
                return candidate;
            }
            cursor = cursor.parent_path();
        }
        throw std::runtime_error("canonical rect_diag PathBundle fixture not found");
    }

    std::filesystem::path path_;
};

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

std::shared_ptr<IConfigurationPort> CreateCanonicalMachineConfigPort(const std::filesystem::path& workspace_root) {
    return std::make_shared<ConfigFileAdapter>(
        (workspace_root / "config" / "machine" / "machine_config.ini").string());
}

std::shared_ptr<FakeProductionBaselinePort> CreateCurrentProductionBaselinePort(
    const std::shared_ptr<IConfigurationPort>& config_port) {
    auto baseline_port = std::make_shared<FakeProductionBaselinePort>();
    const auto config_result = config_port->LoadConfiguration();
    if (config_result.IsError()) {
        return baseline_port;
    }

    const auto& config = config_result.Value();
    baseline_port->baseline_id = "current-production-baseline";
    baseline_port->policy.direction_mode = PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    baseline_port->policy.trigger_spatial_interval_mm = config.dispensing.dot_diameter_target_mm;
    if (config.dispensing.dot_edge_gap_mm > 0.0f) {
        baseline_port->policy.trigger_spatial_interval_mm += config.dispensing.dot_edge_gap_mm;
    }

    std::ostringstream fingerprint;
    fingerprint << "current-production-baseline|approach_direction|"
                << std::fixed << std::setprecision(6)
                << baseline_port->policy.trigger_spatial_interval_mm;
    baseline_port->baseline_fingerprint = fingerprint.str();
    return baseline_port;
}

class ScopedTempDxfCopy {
   public:
    explicit ScopedTempDxfCopy(const std::filesystem::path& source_path) {
        root_ = std::filesystem::temp_directory_path() /
                ("siligen-demo1-" + std::to_string(std::rand()));
        std::filesystem::create_directories(root_);
        dxf_path_ = root_ / source_path.filename();
        std::filesystem::copy_file(
            source_path,
            dxf_path_,
            std::filesystem::copy_options::overwrite_existing);
    }

    ~ScopedTempDxfCopy() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return dxf_path_; }
    [[nodiscard]] std::string string() const { return dxf_path_.string(); }

   private:
    std::filesystem::path root_;
    std::filesystem::path dxf_path_;
};

void ConfigureDemo1ProductionLikeConfig(const std::shared_ptr<FakeConfigurationPort>& config_port) {
    ASSERT_NE(config_port, nullptr);

    config_port->machine_config.max_speed = 300.0f;
    config_port->machine_config.max_acceleration = 1000.0f;
    config_port->machine_config.pulse_per_mm = 200.0f;
    config_port->machine_config.soft_limits.x_min = 0.0f;
    config_port->machine_config.soft_limits.x_max = 480.0f;
    config_port->machine_config.soft_limits.y_min = 0.0f;
    config_port->machine_config.soft_limits.y_max = 480.0f;

    config_port->dispensing_config.strategy = Siligen::Shared::Types::DispensingStrategy::SUBSEGMENTED;
    config_port->dispensing_config.subsegment_count = 8;
    config_port->dispensing_config.dispense_only_cruise = false;
    config_port->dispensing_config.dot_diameter_target_mm = 1.0f;
    config_port->dispensing_config.dot_edge_gap_mm = 2.0f;
    config_port->dispensing_config.trajectory_sample_dt = 0.01f;

    config_port->dispenser_config.cmp_axis_mask = 0x03;
    config_port->dispenser_config.max_count = 1000;

    config_port->valve_coordination_config.dispensing_interval_ms = 100;
    config_port->valve_coordination_config.dispensing_duration_ms = 15;
    config_port->valve_coordination_config.valve_response_ms = 5;
    config_port->valve_coordination_config.visual_margin_ms = 10;
    config_port->valve_coordination_config.min_interval_ms = 10;

    config_port->SyncSystemConfig();
}

void ConfigureProfileCompareRuntimeContractConfig(const std::shared_ptr<FakeConfigurationPort>& config_port) {
    ASSERT_NE(config_port, nullptr);

    config_port->machine_config.pulse_per_mm = 200.0f;
    config_port->dispenser_config.cmp_axis_mask = 0x03;
    config_port->dispenser_config.max_count = 1000;
    config_port->SyncSystemConfig();
}

PlanningRequest BuildGovernedDxfGatewayLikePlanningRequest() {
    PlanningRequest request;
    request.trajectory_config = Siligen::Shared::Types::TrajectoryConfig();
    request.trajectory_config.max_velocity = 10.0f;
    request.optimize_path = true;
    request.start_x = 0.0f;
    request.start_y = 0.0f;
    request.two_opt_iterations = 0;
    request.curve_flatten_max_step_mm = 0.0f;
    request.curve_flatten_max_error_mm = 0.0f;
    request.continuity_tolerance_mm = 0.0f;
    request.curve_chain_angle_deg = 0.0f;
    request.curve_chain_max_segment_mm = 0.0f;
    request.use_interpolation_planner = true;
    request.interpolation_algorithm = Siligen::MotionPlanning::Contracts::InterpolationAlgorithm::LINEAR;
    return request;
}

PreparePlanRuntimeOverrides BuildDemo1ProductionValidationRuntimeOverrides() {
    PreparePlanRuntimeOverrides overrides;
    overrides.dry_run = false;
    overrides.dispensing_speed_mm_s = 10.0f;
    overrides.rapid_speed_mm_s = 10.0f;
    overrides.velocity_trace_enabled = false;
    return overrides;
}

DxfInputQuality ToUploadInputQuality(
    const Siligen::Application::Services::DXF::InputQualitySummary& summary);

class TempUploadStoragePort final : public IUploadStoragePort {
   public:
    explicit TempUploadStoragePort(std::filesystem::path root) : root_(std::move(root)) {
        std::filesystem::create_directories(root_);
    }

    Result<void> Validate(
        const UploadRequest& request,
        size_t max_file_size_mb,
        const std::vector<std::string>& allowed_extensions) const override {
        if (!request.Validate()) {
            return Result<void>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::INVALID_PARAMETER,
                    "upload request is invalid",
                    "TempUploadStoragePort"));
        }
        if (max_file_size_mb > 0U) {
            const auto max_bytes = max_file_size_mb * 1024ULL * 1024ULL;
            if (request.file_size > max_bytes) {
                return Result<void>::Failure(
                    Siligen::Shared::Types::Error(
                        ErrorCode::INVALID_PARAMETER,
                        "upload file exceeds max size",
                        "TempUploadStoragePort"));
            }
        }

        auto extension = std::filesystem::path(request.original_filename).extension().string();
        if (!extension.empty() && extension.front() == '.') {
            extension.erase(extension.begin());
        }
        if (!allowed_extensions.empty() &&
            std::find(allowed_extensions.begin(), allowed_extensions.end(), extension) == allowed_extensions.end()) {
            return Result<void>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::INVALID_PARAMETER,
                    "upload file extension is not allowed",
                    "TempUploadStoragePort"));
        }
        return Result<void>::Success();
    }

    Result<std::string> Store(const UploadRequest& request, const std::string& target_filename) override {
        std::filesystem::create_directories(root_);
        const auto target_path = root_ / target_filename;
        std::ofstream output(target_path, std::ios::binary);
        if (!output.is_open()) {
            return Result<std::string>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::FILE_IO_ERROR,
                    "failed to open upload target file",
                    "TempUploadStoragePort"));
        }
        output.write(
            reinterpret_cast<const char*>(request.file_content.data()),
            static_cast<std::streamsize>(request.file_content.size()));
        output.close();
        if (!output) {
            return Result<std::string>::Failure(
                Siligen::Shared::Types::Error(
                    ErrorCode::FILE_IO_ERROR,
                    "failed to write upload file",
                    "TempUploadStoragePort"));
        }
        return Result<std::string>::Success(target_path.string());
    }

    Result<void> Delete(const std::string& stored_path) override {
        std::error_code ec;
        std::filesystem::remove(stored_path, ec);
        return Result<void>::Success();
    }

   private:
    std::filesystem::path root_;
};

class RuntimeLikeUploadPort final : public IUploadFilePort {
   public:
    explicit RuntimeLikeUploadPort(const std::shared_ptr<IConfigurationPort>&)
        : root_(std::filesystem::temp_directory_path() / ("siligen-upload-" + std::to_string(std::rand()))),
          storage_port_(std::make_shared<TempUploadStoragePort>(root_ / "uploads" / "dxf")),
          use_case_(storage_port_, 10) {}

    ~RuntimeLikeUploadPort() override {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    Result<SourceDrawing> Execute(const UploadRequest& request) override {
        return use_case_.Execute(request);
    }

   private:
    std::filesystem::path root_;
    std::shared_ptr<TempUploadStoragePort> storage_port_;
    UploadFileUseCase use_case_;
};

std::shared_ptr<IUploadFilePort> CreateRuntimeLikeUploadPort(const std::shared_ptr<IConfigurationPort>& config_port) {
    return std::make_shared<RuntimeLikeUploadPort>(config_port);
}

UploadRequest BuildUploadRequestFromFile(const std::filesystem::path& filepath) {
    std::ifstream input(filepath, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("failed to open upload fixture file");
    }
    std::vector<uint8_t> bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    UploadRequest request;
    request.file_content = std::move(bytes);
    request.original_filename = filepath.filename().string();
    request.file_size = request.file_content.size();
    request.content_type = "application/dxf";
    return request;
}

DxfInputQuality ToUploadInputQuality(
    const Siligen::Application::Services::DXF::InputQualitySummary& summary) {
    DxfInputQuality input_quality;
    input_quality.report_id = summary.report_id;
    input_quality.report_path = summary.report_path;
    input_quality.schema_version = summary.schema_version;
    input_quality.dxf_hash = summary.dxf_hash;
    input_quality.source_drawing_ref = summary.source_drawing_ref;
    input_quality.gate_result = summary.gate_result;
    input_quality.classification = summary.classification;
    input_quality.preview_ready = summary.preview_ready;
    input_quality.production_ready = summary.production_ready;
    input_quality.summary = summary.summary;
    input_quality.primary_code = summary.primary_code;
    input_quality.warning_codes = summary.warning_codes;
    input_quality.error_codes = summary.error_codes;
    input_quality.resolved_units = summary.resolved_units;
    input_quality.resolved_unit_scale = summary.resolved_unit_scale;
    return input_quality;
}

PlanningRequest BuildCanonicalPlanningRequest(const std::string& filepath = "canonical.dxf") {
    PlanningRequest request;
    request.dxf_filepath = filepath;
    request.optimize_path = true;
    request.start_x = 0.0f;
    request.start_y = 0.0f;
    request.two_opt_iterations = 7;
    request.curve_flatten_max_step_mm = 0.25f;
    request.curve_flatten_max_error_mm = 0.05f;
    request.continuity_tolerance_mm = 0.02f;
    request.curve_chain_angle_deg = 15.0f;
    request.curve_chain_max_segment_mm = 1.5f;
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
    plan.interpolation_points.front().timestamp = 0.0f;
    plan.interpolation_points.back().timestamp = 1.0f;
    plan.interpolation_points.front().enable_position_trigger = true;
    plan.interpolation_points.back().enable_position_trigger = true;
    plan.trigger_distances_mm = {0.0f, 20.0f};
    return plan;
}

ProfileCompareProgramSpan MakeProgramSpan(
    uint32 trigger_begin_index,
    uint32 trigger_end_index,
    float32 start_profile_position_mm,
    float32 end_profile_position_mm,
    const Point2D& start_position_mm,
    const Point2D& end_position_mm,
    short compare_source_axis,
    uint32 start_boundary_trigger_count) {
    ProfileCompareProgramSpan span;
    span.trigger_begin_index = trigger_begin_index;
    span.trigger_end_index = trigger_end_index;
    span.compare_source_axis = compare_source_axis;
    span.start_boundary_trigger_count = start_boundary_trigger_count;
    span.start_profile_position_mm = start_profile_position_mm;
    span.end_profile_position_mm = end_profile_position_mm;
    span.start_position_mm = start_position_mm;
    span.end_position_mm = end_position_mm;
    return span;
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
    built.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;
    built.execution_plan.profile_compare_program.expected_trigger_count = 2U;
    built.execution_plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{0.0f, 0.0f}, 2000U},
        {1U, 20.0f, Point2D{20.0f, 0.0f}, 2000U},
    };
    built.execution_plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 20.0f, Point2D{0.0f, 0.0f}, Point2D{20.0f, 0.0f}, 1, 1U),
    };
    built.execution_plan.completion_contract.enabled = true;
    built.execution_plan.completion_contract.final_target_position_mm = Point2D{20.0f, 0.0f};
    built.execution_plan.completion_contract.final_position_tolerance_mm = 0.2f;
    built.execution_plan.completion_contract.expected_trigger_count = 2U;
    built.total_length_mm = plan.total_length_mm;
    built.execution_nominal_time_s = plan.estimated_time_s;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "artifact-fingerprint";
    return Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated(built);
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildAxisSwitchExecutionPackage() {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    const std::vector<Point2D> trigger_points{
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 100.0f)};

    ExecutionPackageBuilt built;
    built.execution_plan.total_length_mm = 200.0f;
    built.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;

    float32 cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < trigger_points.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm += static_cast<float32>(trigger_points[index - 1U].DistanceTo(trigger_points[index]));
        }

        Siligen::TrajectoryPoint interpolation_point;
        interpolation_point.position = trigger_points[index];
        interpolation_point.sequence_id = static_cast<uint32>(index);
        interpolation_point.timestamp = static_cast<float32>(index) * 0.1f;
        interpolation_point.velocity = 20.0f;
        interpolation_point.enable_position_trigger = true;
        interpolation_point.trigger_position_mm = cumulative_length_mm;
        interpolation_point.trigger_pulse_width_us = 2000U;
        built.execution_plan.interpolation_points.push_back(interpolation_point);

        Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint motion_point;
        motion_point.t = static_cast<float32>(index) * 0.1f;
        motion_point.position = {trigger_points[index].x, trigger_points[index].y, 0.0f};
        motion_point.velocity = {20.0f, 20.0f, 0.0f};
        built.execution_plan.motion_trajectory.points.push_back(motion_point);

        built.execution_plan.profile_compare_program.trigger_points.push_back(
            {static_cast<uint32>(index), cumulative_length_mm, trigger_points[index], 2000U});
        built.execution_plan.trigger_distances_mm.push_back(cumulative_length_mm);

        if (index == 0U) {
            continue;
        }

        InterpolationData segment;
        segment.type = InterpolationType::LINEAR;
        segment.positions = {trigger_points[index].x, trigger_points[index].y};
        segment.velocity = 20.0f;
        segment.acceleration = 100.0f;
        segment.end_velocity = index + 1U == trigger_points.size() ? 0.0f : 20.0f;
        built.execution_plan.interpolation_segments.push_back(segment);
    }

    built.execution_plan.motion_trajectory.total_length = cumulative_length_mm;
    built.execution_plan.motion_trajectory.total_time =
        static_cast<float32>(trigger_points.size() - 1U) * 0.1f;
    built.execution_plan.profile_compare_program.expected_trigger_count =
        static_cast<uint32>(built.execution_plan.profile_compare_program.trigger_points.size());
    built.execution_plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 1U, 0.0f, 100.0f, Point2D{0.0f, 0.0f}, Point2D{100.0f, 0.0f}, 1, 1U),
        MakeProgramSpan(2U, 2U, 100.0f, 200.0f, Point2D{100.0f, 0.0f}, Point2D{100.0f, 100.0f}, 2, 0U),
    };
    built.execution_plan.completion_contract.enabled = true;
    built.execution_plan.completion_contract.final_target_position_mm = trigger_points.back();
    built.execution_plan.completion_contract.final_position_tolerance_mm = 0.2f;
    built.execution_plan.completion_contract.expected_trigger_count =
        built.execution_plan.profile_compare_program.expected_trigger_count;
    built.total_length_mm = cumulative_length_mm;
    built.execution_nominal_time_s = built.execution_plan.motion_trajectory.total_time;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "axis-switch";
    return ExecutionPackageValidated(std::move(built));
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildInterpolationAnchoredExecutionPackage() {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    ExecutionPackageBuilt built;
    built.execution_plan.total_length_mm = 10.0f;
    built.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;

    const std::vector<Point2D> trigger_points{
        Point2D(10.0f, 0.0f),
        Point2D(15.0f, 0.0f),
        Point2D(20.0f, 0.0f)};

    for (std::size_t index = 0; index < trigger_points.size(); ++index) {
        Siligen::TrajectoryPoint interpolation_point;
        interpolation_point.position = trigger_points[index];
        interpolation_point.sequence_id = static_cast<uint32>(index);
        interpolation_point.timestamp = static_cast<float32>(index) * 0.1f;
        interpolation_point.velocity = 20.0f;
        interpolation_point.enable_position_trigger = true;
        interpolation_point.trigger_position_mm = static_cast<float32>(index) * 5.0f;
        interpolation_point.trigger_pulse_width_us = 2000U;
        built.execution_plan.interpolation_points.push_back(interpolation_point);
        built.execution_plan.profile_compare_program.trigger_points.push_back(
            {static_cast<uint32>(index),
             static_cast<float32>(index) * 5.0f,
             trigger_points[index],
             2000U});
        built.execution_plan.trigger_distances_mm.push_back(static_cast<float32>(index) * 5.0f);

        if (index == 0U) {
            continue;
        }

        InterpolationData segment;
        segment.type = InterpolationType::LINEAR;
        segment.positions = {trigger_points[index].x, trigger_points[index].y};
        segment.velocity = 20.0f;
        segment.acceleration = 100.0f;
        segment.end_velocity = index + 1U == trigger_points.size() ? 0.0f : 20.0f;
        built.execution_plan.interpolation_segments.push_back(segment);
    }

    {
        MotionTrajectoryPoint motion_point;
        motion_point.t = 0.0f;
        motion_point.position = {0.0f, 0.0f, 0.0f};
        motion_point.velocity = {20.0f, 0.0f, 0.0f};
        built.execution_plan.motion_trajectory.points.push_back(motion_point);
    }
    for (std::size_t index = 0; index < trigger_points.size(); ++index) {
        MotionTrajectoryPoint motion_point;
        motion_point.t = static_cast<float32>(index + 1U) * 0.1f;
        motion_point.position = {trigger_points[index].x, trigger_points[index].y, 0.0f};
        motion_point.velocity = {20.0f, 0.0f, 0.0f};
        built.execution_plan.motion_trajectory.points.push_back(motion_point);
    }
    built.execution_plan.motion_trajectory.total_length = 0.0f;
    built.execution_plan.motion_trajectory.total_time = 0.3f;

    built.execution_plan.profile_compare_program.expected_trigger_count =
        static_cast<uint32>(built.execution_plan.profile_compare_program.trigger_points.size());
    built.execution_plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 2U, 0.0f, 10.0f, Point2D{10.0f, 0.0f}, Point2D{20.0f, 0.0f}, 1, 1U),
    };
    built.execution_plan.completion_contract.enabled = true;
    built.execution_plan.completion_contract.final_target_position_mm = trigger_points.back();
    built.execution_plan.completion_contract.final_position_tolerance_mm = 0.2f;
    built.execution_plan.completion_contract.expected_trigger_count =
        built.execution_plan.profile_compare_program.expected_trigger_count;
    built.total_length_mm = built.execution_plan.total_length_mm;
    built.execution_nominal_time_s = built.execution_plan.motion_trajectory.total_time;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "interpolation-anchored";
    return ExecutionPackageValidated(std::move(built));
}

Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated BuildClosedLoopBranchRevisitExecutionPackage() {
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageBuilt;
    using Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated;

    const std::vector<Point2D> trigger_points{
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 100.0f),
        Point2D(0.0f, 100.0f),
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 100.0f)};

    ExecutionPackageBuilt built;
    built.execution_plan.total_length_mm = 541.4214f;
    built.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;

    float32 cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < trigger_points.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm += static_cast<float32>(trigger_points[index - 1U].DistanceTo(trigger_points[index]));
        }

        Siligen::TrajectoryPoint interpolation_point;
        interpolation_point.position = trigger_points[index];
        interpolation_point.sequence_id = static_cast<uint32>(index);
        interpolation_point.timestamp = static_cast<float32>(index) * 0.1f;
        interpolation_point.velocity = 20.0f;
        interpolation_point.enable_position_trigger = true;
        interpolation_point.trigger_position_mm = cumulative_length_mm;
        interpolation_point.trigger_pulse_width_us = 2000U;
        built.execution_plan.interpolation_points.push_back(interpolation_point);

        Siligen::Domain::Motion::ValueObjects::MotionTrajectoryPoint motion_point;
        motion_point.t = static_cast<float32>(index) * 0.1f;
        motion_point.position = {trigger_points[index].x, trigger_points[index].y, 0.0f};
        motion_point.velocity = {20.0f, 20.0f, 0.0f};
        built.execution_plan.motion_trajectory.points.push_back(motion_point);

        built.execution_plan.profile_compare_program.trigger_points.push_back(
            {static_cast<uint32>(index), cumulative_length_mm, trigger_points[index], 2000U});
        built.execution_plan.trigger_distances_mm.push_back(cumulative_length_mm);

        if (index == 0U) {
            continue;
        }

        InterpolationData segment;
        segment.type = InterpolationType::LINEAR;
        segment.positions = {trigger_points[index].x, trigger_points[index].y};
        segment.velocity = 20.0f;
        segment.acceleration = 100.0f;
        segment.end_velocity = index + 1U == trigger_points.size() ? 0.0f : 20.0f;
        built.execution_plan.interpolation_segments.push_back(segment);
    }

    built.execution_plan.motion_trajectory.total_length = cumulative_length_mm;
    built.execution_plan.motion_trajectory.total_time =
        static_cast<float32>(trigger_points.size() - 1U) * 0.1f;
    built.execution_plan.profile_compare_program.expected_trigger_count =
        static_cast<uint32>(built.execution_plan.profile_compare_program.trigger_points.size());
    built.execution_plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 5U, 0.0f, cumulative_length_mm, Point2D{0.0f, 0.0f}, Point2D{10.0f, 10.0f}, 1, 1U),
    };
    built.execution_plan.completion_contract.enabled = true;
    built.execution_plan.completion_contract.final_target_position_mm = trigger_points.back();
    built.execution_plan.completion_contract.final_position_tolerance_mm = 0.2f;
    built.execution_plan.completion_contract.expected_trigger_count =
        built.execution_plan.profile_compare_program.expected_trigger_count;
    built.total_length_mm = cumulative_length_mm;
    built.execution_nominal_time_s = built.execution_plan.motion_trajectory.total_time;
    built.source_path = "artifact.pb";
    built.source_fingerprint = "branch-revisit";
    return ExecutionPackageValidated(std::move(built));
}

void ApplyOwnerExecutionContractState(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    plan_record.execution_contract_ready = true;
    plan_record.execution_failure_reason.clear();
    plan_record.execution_diagnostic_code.clear();
    plan_record.execution_assembly.execution_contract_ready = true;
    plan_record.execution_assembly.execution_failure_reason.clear();
    plan_record.execution_assembly.execution_diagnostic_code.clear();
    plan_record.execution_assembly.formal_compare_gate = {};

    if (!plan_record.execution_launch.execution_package) {
        return;
    }

    const auto& execution_plan = plan_record.execution_launch.execution_package->execution_plan;
    if (execution_plan.production_trigger_mode !=
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE) {
        return;
    }

    const auto compile_result = CompileProfileCompareExecutionSchedule(
        ProfileCompareExecutionCompileInput{
            execution_plan,
            ProfileCompareRuntimeContract{200.0f, 0x03, 1000U},
        });
    if (compile_result.IsSuccess()) {
        return;
    }

    plan_record.execution_contract_ready = false;
    plan_record.execution_failure_reason =
        "owner execution package 不满足 formal runtime compare contract: " +
        compile_result.GetError().GetMessage();
    plan_record.execution_diagnostic_code = "formal_runtime_compare_contract_unsatisfied";
    plan_record.execution_assembly.execution_contract_ready = false;
    plan_record.execution_assembly.execution_failure_reason = plan_record.execution_failure_reason;
    plan_record.execution_assembly.execution_diagnostic_code = plan_record.execution_diagnostic_code;
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

std::shared_ptr<PlanningUseCase> CreateRealPlanningUseCase(
    const std::shared_ptr<IConfigurationPort>& config_port = nullptr) {
    auto path_source = std::make_shared<LinePathSourceStub>();
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        config_port,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service));
}

std::shared_ptr<PlanningUseCase> CreatePlanningUseCaseWithPathSourceAndExport(
    const std::shared_ptr<Siligen::ProcessPath::Contracts::IPathSourcePort>& path_source,
    const std::shared_ptr<IPlanningArtifactExportPort>& export_port,
    const std::shared_ptr<IConfigurationPort>& config_port = nullptr) {
    auto pb_service = std::make_shared<DxfPbPreparationService>();
    return std::make_shared<PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>()),
        CreatePlanningOperations(),
        config_port,
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
        const Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated& execution_package,
        const DispensingRuntimeParams&,
        const DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        IDispensingExecutionObserver* observer = nullptr) noexcept override {
        const auto& plan = execution_package.execution_plan;
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
        if (options.expected_trace && !options.expected_trace->Empty()) {
            report.traceability_verdict = "passed";
            report.strict_one_to_one_proven = true;
            report.actual_trace.reserve(options.expected_trace->items.size());
            for (std::size_t index = 0; index < options.expected_trace->items.size(); ++index) {
                const auto& expected_item = options.expected_trace->items[index];
                Siligen::Domain::Dispensing::ValueObjects::ProfileCompareActualTraceItem actual_item;
                actual_item.cycle_index = expected_item.cycle_index;
                actual_item.trigger_sequence_id = expected_item.trigger_sequence_id;
                actual_item.completion_sequence = static_cast<uint32>(index + 1U);
                actual_item.span_index = expected_item.span_index;
                actual_item.local_completed_trigger_count = static_cast<uint32>(index + 1U);
                actual_item.observed_completed_trigger_count = static_cast<uint32>(index + 1U);
                actual_item.compare_source_axis = expected_item.compare_source_axis;
                actual_item.compare_position_pulse = expected_item.compare_position_pulse;
                actual_item.authority_trigger_ref = expected_item.authority_trigger_ref;
                actual_item.trigger_mode = expected_item.trigger_mode;
                report.actual_trace.push_back(std::move(actual_item));
            }
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
    plan_record.response.path_quality = Siligen::Shared::Types::PathQualityAssessment{};
    plan_record.path_quality = plan_record.response.path_quality;
    plan_record.authority_trigger_layout.layout_id = layout_id;
    plan_record.authority_trigger_layout.plan_id = plan_record.response.plan_id;
    plan_record.authority_trigger_layout.plan_fingerprint = plan_record.response.plan_fingerprint;
    plan_record.authority_trigger_layout.authority_ready = true;
    plan_record.authority_trigger_layout.binding_ready = true;
    plan_record.authority_trigger_layout.state =
        Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState::BindingReady;
}

void SeedAuthorityTriggerLayoutFromExecutionPackage(
    DispensingWorkflowUseCase::PlanRecord& plan_record,
    const Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated& execution_package) {
    const auto& execution_plan = execution_package.execution_plan;
    const auto& owner_program = execution_plan.profile_compare_program;

    plan_record.authority_trigger_layout.trigger_points.clear();
    plan_record.authority_trigger_layout.bindings.clear();
    plan_record.authority_trigger_layout.spans.clear();
    plan_record.authority_trigger_layout.trigger_points.reserve(owner_program.trigger_points.size());
    plan_record.authority_trigger_layout.bindings.reserve(owner_program.trigger_points.size());
    plan_record.authority_trigger_layout.spans.reserve(owner_program.spans.size());

    for (std::size_t span_index = 0; span_index < owner_program.spans.size(); ++span_index) {
        const auto& owner_span = owner_program.spans[span_index];
        Siligen::Domain::Dispensing::ValueObjects::DispenseSpan span;
        span.span_id = "span-" + std::to_string(span_index);
        span.layout_ref = plan_record.authority_trigger_layout.layout_id;
        span.component_id = "component-0";
        span.component_index = 0U;
        span.order_index = span_index;
        span.total_length_mm = owner_span.end_profile_position_mm - owner_span.start_profile_position_mm;
        span.validation_state = Siligen::Domain::Dispensing::ValueObjects::SpacingValidationClassification::Pass;
        span.anchor_constraints_satisfied = true;
        plan_record.authority_trigger_layout.spans.push_back(std::move(span));
    }

    for (std::size_t trigger_index = 0; trigger_index < owner_program.trigger_points.size(); ++trigger_index) {
        const auto& owner_trigger = owner_program.trigger_points[trigger_index];
        std::size_t span_index = 0U;
        for (; span_index < owner_program.spans.size(); ++span_index) {
            const auto& owner_span = owner_program.spans[span_index];
            if (trigger_index >= owner_span.trigger_begin_index && trigger_index <= owner_span.trigger_end_index) {
                break;
            }
        }
        if (span_index >= owner_program.spans.size()) {
            span_index = owner_program.spans.empty() ? 0U : owner_program.spans.size() - 1U;
        }

        const auto& owner_span = owner_program.spans[span_index];
        Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint trigger_point;
        trigger_point.trigger_id = "trigger-" + std::to_string(trigger_index);
        trigger_point.layout_ref = plan_record.authority_trigger_layout.layout_id;
        trigger_point.span_ref = plan_record.authority_trigger_layout.spans[span_index].span_id;
        trigger_point.sequence_index_global = trigger_index;
        trigger_point.sequence_index_span = trigger_index - owner_span.trigger_begin_index;
        trigger_point.distance_mm_global = owner_trigger.profile_position_mm;
        trigger_point.distance_mm_span = owner_trigger.profile_position_mm - owner_span.start_profile_position_mm;
        trigger_point.position = owner_trigger.trigger_position_mm;
        trigger_point.source_segment_index = trigger_index > 0U ? trigger_index - 1U : 0U;
        plan_record.authority_trigger_layout.trigger_points.push_back(trigger_point);

        Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding binding;
        binding.binding_id =
            plan_record.authority_trigger_layout.layout_id + "-binding-" + std::to_string(trigger_index);
        binding.layout_ref = plan_record.authority_trigger_layout.layout_id;
        binding.trigger_ref = trigger_point.trigger_id;
        binding.interpolation_index = trigger_index;
        binding.execution_position =
            trigger_index < execution_plan.interpolation_points.size()
            ? execution_plan.interpolation_points[trigger_index].position
            : owner_trigger.trigger_position_mm;
        binding.match_error_mm = 0.0f;
        binding.monotonic = true;
        binding.bound = true;
        plan_record.authority_trigger_layout.bindings.push_back(binding);
    }

    if (!owner_program.trigger_points.empty()) {
        plan_record.response.total_length_mm = owner_program.trigger_points.back().profile_position_mm;
    }
}

void SeedAuthorityTriggerPoints(
    DispensingWorkflowUseCase::PlanRecord& plan_record,
    const std::vector<Point2D>& points) {
    auto& authority_layout = plan_record.authority_trigger_layout;
    authority_layout.trigger_points.clear();
    authority_layout.bindings.clear();
    authority_layout.spans.clear();
    authority_layout.components.clear();
    authority_layout.validation_outcomes.clear();
    authority_layout.effective_component_count = 0U;
    authority_layout.ignored_component_count = 0U;

    const auto execution_package = plan_record.execution_launch.execution_package;
    if (execution_package &&
        execution_package->execution_plan.production_trigger_mode ==
            Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE &&
        !execution_package->execution_plan.profile_compare_program.Empty()) {
        const auto& execution_plan = execution_package->execution_plan;
        const auto& program = execution_plan.profile_compare_program;
        authority_layout.state =
            Siligen::Domain::Dispensing::ValueObjects::AuthorityTriggerLayoutState::BindingReady;
        authority_layout.trigger_points.reserve(program.trigger_points.size());
        authority_layout.bindings.reserve(program.trigger_points.size());
        authority_layout.spans.reserve(program.spans.size());

        const std::string component_id = authority_layout.layout_id + "-component-0";
        for (std::size_t span_index = 0; span_index < program.spans.size(); ++span_index) {
            const auto& owner_span = program.spans[span_index];

            Siligen::Domain::Dispensing::ValueObjects::DispenseSpan span;
            span.span_id = authority_layout.layout_id + "-span-" + std::to_string(span_index);
            span.layout_ref = authority_layout.layout_id;
            span.component_id = component_id;
            span.component_index = 0U;
            span.order_index = span_index;
            span.source_segment_indices = {span_index};
            span.total_length_mm =
                owner_span.end_profile_position_mm >= owner_span.start_profile_position_mm
                ? owner_span.end_profile_position_mm - owner_span.start_profile_position_mm
                : 0.0f;
            span.interval_count =
                owner_span.ExpectedTriggerCount() > 0U ? owner_span.ExpectedTriggerCount() - 1U : 0U;
            span.anchor_constraints_satisfied = true;
            authority_layout.spans.push_back(span);

            for (uint32 trigger_index = owner_span.trigger_begin_index;
                 trigger_index <= owner_span.trigger_end_index;
                 ++trigger_index) {
                const auto& owner_trigger = program.trigger_points[trigger_index];
                const auto local_trigger_index =
                    static_cast<std::size_t>(trigger_index - owner_span.trigger_begin_index);

                Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint trigger_point;
                trigger_point.trigger_id = authority_layout.layout_id + "-trigger-" + std::to_string(trigger_index);
                trigger_point.layout_ref = authority_layout.layout_id;
                trigger_point.span_ref = span.span_id;
                trigger_point.sequence_index_global = trigger_index;
                trigger_point.sequence_index_span = local_trigger_index;
                trigger_point.distance_mm_global = owner_trigger.profile_position_mm;
                trigger_point.distance_mm_span =
                    owner_trigger.profile_position_mm - owner_span.start_profile_position_mm;
                trigger_point.position = owner_trigger.trigger_position_mm;
                trigger_point.source_segment_index = span.source_segment_indices.front();
                authority_layout.trigger_points.push_back(trigger_point);

                Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding binding;
                binding.binding_id = authority_layout.layout_id + "-binding-" + std::to_string(trigger_index);
                binding.layout_ref = authority_layout.layout_id;
                binding.trigger_ref = trigger_point.trigger_id;

                std::size_t binding_index = 0U;
                if (!execution_plan.interpolation_points.empty()) {
                    binding_index = std::min<std::size_t>(
                        static_cast<std::size_t>(trigger_index),
                        execution_plan.interpolation_points.size() - 1U);
                    auto best_distance = static_cast<float>(
                        execution_plan.interpolation_points[binding_index].position.DistanceTo(
                            owner_trigger.trigger_position_mm));
                    for (std::size_t interpolation_index = 0U;
                         interpolation_index < execution_plan.interpolation_points.size();
                         ++interpolation_index) {
                        const auto distance = static_cast<float>(
                            execution_plan.interpolation_points[interpolation_index].position.DistanceTo(
                                owner_trigger.trigger_position_mm));
                        if (distance < best_distance) {
                            best_distance = distance;
                            binding_index = interpolation_index;
                        }
                    }
                    binding.execution_position = execution_plan.interpolation_points[binding_index].position;
                    binding.match_error_mm = best_distance;
                    binding.bound = true;
                } else {
                    binding.execution_position = owner_trigger.trigger_position_mm;
                    binding.match_error_mm = 0.0f;
                    binding.bound = false;
                }

                binding.interpolation_index = binding_index;
                binding.monotonic = true;
                authority_layout.bindings.push_back(binding);
            }
        }

        if (!authority_layout.spans.empty()) {
            Siligen::Domain::Dispensing::ValueObjects::AuthorityLayoutComponent component;
            component.component_id = component_id;
            component.layout_ref = authority_layout.layout_id;
            component.component_index = 0U;
            component.span_refs.reserve(authority_layout.spans.size());
            component.source_segment_indices.reserve(authority_layout.spans.size());
            for (const auto& span : authority_layout.spans) {
                component.span_refs.push_back(span.span_id);
                component.source_segment_indices.insert(
                    component.source_segment_indices.end(),
                    span.source_segment_indices.begin(),
                    span.source_segment_indices.end());
            }
            authority_layout.components.push_back(std::move(component));
            authority_layout.effective_component_count = 1U;
        }

        plan_record.response.total_length_mm =
            execution_package->total_length_mm > 0.0f
            ? execution_package->total_length_mm
            : execution_plan.total_length_mm;
        return;
    }

    authority_layout.trigger_points.reserve(points.size());
    authority_layout.bindings.reserve(points.size());
    float cumulative_length_mm = 0.0f;
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (index > 0U) {
            cumulative_length_mm += static_cast<float>(points[index - 1U].DistanceTo(points[index]));
        }
        Siligen::Domain::Dispensing::ValueObjects::LayoutTriggerPoint trigger_point;
        trigger_point.trigger_id = "trigger-" + std::to_string(index);
        trigger_point.layout_ref = authority_layout.layout_id;
        trigger_point.sequence_index_global = index;
        trigger_point.distance_mm_global = cumulative_length_mm;
        trigger_point.position = points[index];
        authority_layout.trigger_points.push_back(trigger_point);

        Siligen::Domain::Dispensing::ValueObjects::InterpolationTriggerBinding binding;
        binding.binding_id = authority_layout.layout_id + "-binding-" + std::to_string(index);
        binding.layout_ref = authority_layout.layout_id;
        binding.trigger_ref = trigger_point.trigger_id;
        binding.interpolation_index = index;
        binding.execution_position = points[index];
        binding.match_error_mm = 0.0f;
        binding.monotonic = true;
        binding.bound = true;
        authority_layout.bindings.push_back(binding);
    }
    plan_record.response.total_length_mm = cumulative_length_mm;
}

void SeedPlan(DispensingWorkflowUseCase& use_case, const std::string& plan_id) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    SeedProductionReadyInputQuality(plan_record);
    plan_record.execution_launch.execution_package =
        std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
            BuildMinimalExecutionPackage());
    plan_record.execution_launch.runtime_overrides.source_path = "artifact.pb";
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
    plan_record.response.path_quality = Siligen::Shared::Types::PathQualityAssessment{};
    plan_record.path_quality = plan_record.response.path_quality;
    SeedAuthorityMetadata(plan_record, "layout-" + plan_id);
    plan_record.execution_assembly.success = true;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.execution_assembly.preview_authority_shared_with_execution = true;
    plan_record.execution_assembly.execution_binding_ready = true;
    plan_record.execution_assembly.execution_package = plan_record.execution_launch.execution_package;
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    ApplyOwnerExecutionContractState(plan_record);
    SeedAuthorityTriggerLayoutFromExecutionPackage(plan_record, *plan_record.execution_launch.execution_package);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    use_case.plans_[plan_id] = plan_record;
}

std::shared_ptr<PlanningUseCase> CreatePbBackedPlanningUseCase(
    const std::shared_ptr<IConfigurationPort>& config_port) {
    auto pb_service = std::make_shared<DxfPbPreparationService>(config_port);
    return std::make_shared<PlanningUseCase>(
        std::make_shared<PbPathSourceAdapter>(),
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(
                std::make_shared<Siligen::Domain::Motion::DomainServices::SevenSegmentSCurveProfile>())),
        CreatePlanningOperations(),
        config_port,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(pb_service));
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
    built.execution_plan.production_trigger_mode =
        Siligen::Domain::Dispensing::ValueObjects::ProductionTriggerMode::PROFILE_COMPARE;
    built.execution_plan.profile_compare_program.expected_trigger_count = 1U;
    built.execution_plan.profile_compare_program.trigger_points = {
        {0U, 0.0f, Point2D{5.0f, 5.0f}, 2000U},
    };
    built.execution_plan.profile_compare_program.spans = {
        MakeProgramSpan(0U, 0U, 0.0f, 3.0f, Point2D{5.0f, 5.0f}, Point2D{8.0f, 5.0f}, 1, 1U),
    };
    built.execution_plan.completion_contract.enabled = true;
    built.execution_plan.completion_contract.final_target_position_mm = Point2D{8.0f, 5.0f};
    built.execution_plan.completion_contract.final_position_tolerance_mm = 0.2f;
    built.execution_plan.completion_contract.expected_trigger_count = 1U;
    built.total_length_mm = 3.0f;
    built.execution_nominal_time_s = 0.12f;
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
    SeedAuthorityTriggerLayoutFromExecutionPackage(plan_record, *package);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    ApplyOwnerExecutionContractState(plan_record);
}

void ConfigureAxisSwitchProfileComparePlan(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    auto package = std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
        BuildAxisSwitchExecutionPackage());
    plan_record.execution_launch.execution_package = package;
    plan_record.execution_assembly.execution_package = package;
    plan_record.execution_trajectory_points = package->execution_plan.interpolation_points;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.glue_points = {
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 100.0f),
    };
    plan_record.response.total_length_mm = package->total_length_mm;
    plan_record.response.point_count = static_cast<std::uint32_t>(plan_record.execution_trajectory_points.size());
    plan_record.authority_trigger_layout.branch_revisit_split_applied = false;
    plan_record.authority_trigger_layout.spans.clear();
    SeedAuthorityTriggerLayoutFromExecutionPackage(plan_record, *package);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    ApplyOwnerExecutionContractState(plan_record);
}

void ConfigureClosedLoopBranchRevisitProfileComparePlan(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    auto package = std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
        BuildClosedLoopBranchRevisitExecutionPackage());
    plan_record.execution_launch.execution_package = package;
    plan_record.execution_assembly.execution_package = package;
    plan_record.execution_trajectory_points = package->execution_plan.interpolation_points;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.glue_points = {
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 0.0f),
        Point2D(100.0f, 100.0f),
        Point2D(0.0f, 100.0f),
        Point2D(0.0f, 0.0f),
        Point2D(100.0f, 100.0f),
    };
    plan_record.response.total_length_mm = package->total_length_mm;
    plan_record.response.point_count = static_cast<std::uint32_t>(plan_record.execution_trajectory_points.size());
    plan_record.authority_trigger_layout.branch_revisit_split_applied = true;
    SeedAuthorityTriggerPoints(plan_record, plan_record.glue_points);
    plan_record.authority_trigger_layout.spans.clear();
    {
        Siligen::Domain::Dispensing::ValueObjects::DispenseSpan closed_span;
        closed_span.span_id = "closed-span";
        closed_span.layout_ref = plan_record.authority_trigger_layout.layout_id;
        closed_span.component_id = plan_record.authority_trigger_layout.layout_id + "-component-0";
        closed_span.component_index = 0U;
        closed_span.order_index = 0U;
        closed_span.closed = true;
        plan_record.authority_trigger_layout.spans.push_back(closed_span);
    }
    {
        Siligen::Domain::Dispensing::ValueObjects::DispenseSpan revisit_span;
        revisit_span.span_id = "revisit-span";
        revisit_span.layout_ref = plan_record.authority_trigger_layout.layout_id;
        revisit_span.component_id = plan_record.authority_trigger_layout.layout_id + "-component-0";
        revisit_span.component_index = 0U;
        revisit_span.order_index = 1U;
        revisit_span.split_reason =
            Siligen::Domain::Dispensing::ValueObjects::DispenseSpanSplitReason::BranchOrRevisit;
        plan_record.authority_trigger_layout.spans.push_back(revisit_span);
    }
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    ApplyOwnerExecutionContractState(plan_record);
}

void ConfigureInterpolationAnchoredProfileComparePlan(DispensingWorkflowUseCase::PlanRecord& plan_record) {
    auto package = std::make_shared<Siligen::Domain::Dispensing::Contracts::ExecutionPackageValidated>(
        BuildInterpolationAnchoredExecutionPackage());
    plan_record.execution_launch.execution_package = package;
    plan_record.execution_assembly.execution_package = package;
    plan_record.execution_trajectory_points = package->execution_plan.interpolation_points;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.glue_points = {
        Point2D(10.0f, 0.0f),
        Point2D(15.0f, 0.0f),
        Point2D(20.0f, 0.0f),
    };
    plan_record.response.total_length_mm = package->total_length_mm;
    plan_record.response.point_count = static_cast<std::uint32_t>(plan_record.execution_trajectory_points.size());
    plan_record.authority_trigger_layout.branch_revisit_split_applied = false;
    plan_record.authority_trigger_layout.spans.clear();
    SeedAuthorityTriggerLayoutFromExecutionPackage(plan_record, *package);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
    ApplyOwnerExecutionContractState(plan_record);
}

DispensingWorkflowUseCase CreateUseCase(
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<DispensingExecutionUseCase> execution_use_case = nullptr,
    std::shared_ptr<FakeProductionBaselinePort> baseline_policy_port =
        std::make_shared<FakeProductionBaselinePort>(),
    std::shared_ptr<IConfigurationPort> config_port =
        std::make_shared<FakeConfigurationPort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    auto execution_port = execution_use_case
        ? CreateRuntimeWorkflowExecutionPort(std::move(execution_use_case))
        : MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        MakeDummyShared<PlanningUseCase>(),
        baseline_policy_port,
        execution_port,
        config_port,
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
    std::shared_ptr<FakeProductionBaselinePort> baseline_policy_port =
        std::make_shared<FakeProductionBaselinePort>(),
    std::shared_ptr<IConfigurationPort> config_port =
        std::make_shared<FakeConfigurationPort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        baseline_policy_port,
        MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>(),
        config_port,
        connection_port,
        motion_device_port,
        dispenser_device_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase CreateUseCaseWithUploadAndPlanning(
    const std::shared_ptr<IUploadFilePort>& upload_port,
    const std::shared_ptr<PlanningUseCase>& planning_use_case,
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<FakeProductionBaselinePort> baseline_policy_port =
        std::make_shared<FakeProductionBaselinePort>(),
    std::shared_ptr<IConfigurationPort> config_port =
        std::make_shared<FakeConfigurationPort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        upload_port,
        planning_use_case,
        baseline_policy_port,
        MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>(),
        config_port,
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
    std::shared_ptr<FakeProductionBaselinePort> baseline_policy_port =
        std::make_shared<FakeProductionBaselinePort>(),
    std::shared_ptr<IConfigurationPort> config_port =
        std::make_shared<FakeConfigurationPort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        baseline_policy_port,
        CreateRuntimeWorkflowExecutionPort(execution_use_case),
        config_port,
        connection_port,
        motion_device_port,
        dispenser_device_port,
        motion_state_port,
        homing_port,
        interlock_port);
}

DispensingWorkflowUseCase CreateUseCaseWithPlanningAndExecutionPort(
    const std::shared_ptr<PlanningUseCase>& planning_use_case,
    const std::shared_ptr<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>& execution_port,
    const std::shared_ptr<FakeHardwareConnectionPort>& connection_port,
    const std::shared_ptr<FakeMotionStatePort>& motion_state_port,
    const std::shared_ptr<FakeHomingPort>& homing_port,
    const std::shared_ptr<FakeInterlockSignalPort>& interlock_port,
    std::shared_ptr<FakeProductionBaselinePort> baseline_policy_port =
        std::make_shared<FakeProductionBaselinePort>(),
    std::shared_ptr<IConfigurationPort> config_port =
        std::make_shared<FakeConfigurationPort>()) {
    auto motion_device_port = std::make_shared<FakeMotionDevicePort>();
    auto dispenser_device_port = std::make_shared<FakeDispenserDevicePort>();
    return DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        baseline_policy_port,
        execution_port,
        config_port,
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

Siligen::ProcessPath::Contracts::ProcessPath BuildProcessPathFromPolyline(
    const std::vector<Point2D>& points);

DispensingWorkflowUseCase::PlanRecord BuildPreviewPlanRecord(
    const std::string& plan_id,
    const std::vector<Point2D>& points) {
    DispensingWorkflowUseCase::PlanRecord plan_record;
    plan_record.response.plan_id = plan_id;
    plan_record.response.plan_fingerprint = "fp-" + plan_id;
    plan_record.response.segment_count = static_cast<std::uint32_t>(points.size() > 1 ? points.size() - 1 : 0);
    plan_record.response.point_count = static_cast<std::uint32_t>(points.size());
    plan_record.response.total_length_mm = 0.0f;
    plan_record.response.execution_nominal_time_s = 1.0f;
    plan_record.preview_estimated_time_s = 1.0f;
    SeedProductionReadyInputQuality(plan_record);
    plan_record.preview_authority_ready = true;
    plan_record.preview_authority_shared_with_execution = true;
    plan_record.preview_binding_ready = true;
    plan_record.preview_spacing_valid = true;
    plan_record.preview_diagnostic_code.clear();
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::SNAPSHOT_READY;
    plan_record.preview_snapshot_hash = plan_record.response.plan_fingerprint;
    plan_record.latest = true;
    plan_record.preview_snapshot_id = plan_id;
    plan_record.execution_launch.authority_preview.success = true;
    plan_record.execution_launch.authority_preview.canonical_execution_process_path =
        BuildProcessPathFromPolyline(points);
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

Siligen::ProcessPath::Contracts::ProcessPath BuildProcessPathFromPolyline(
    const std::vector<Point2D>& points) {
    Siligen::ProcessPath::Contracts::ProcessPath process_path;
    for (std::size_t index = 1; index < points.size(); ++index) {
        process_path.segments.push_back(BuildLineProcessSegment(points[index - 1U], points[index]));
    }
    return process_path;
}

Siligen::Application::Services::Dispensing::WorkflowSpacingValidationGroup BuildSpacingValidationGroup(
    const bool short_segment_exception,
    const bool within_window,
    const std::size_t segment_index = 0U,
    const float actual_spacing_mm = 2.0f) {
    Siligen::Application::Services::Dispensing::WorkflowSpacingValidationGroup group;
    group.segment_index = segment_index;
    group.points = {
        Point2D(static_cast<float>(segment_index), 0.0f),
        Point2D(static_cast<float>(segment_index) + actual_spacing_mm, 0.0f),
    };
    group.actual_spacing_mm = actual_spacing_mm;
    group.short_segment_exception = short_segment_exception;
    group.within_window = within_window;
    return group;
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    ASSERT_TRUE(stored_launch.runtime_overrides.dispensing_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(stored_launch.runtime_overrides.dispensing_speed_mm_s.value(), 22.0f);
    ASSERT_TRUE(stored_launch.runtime_overrides.dry_run_speed_mm_s.has_value());
    EXPECT_FLOAT_EQ(stored_launch.runtime_overrides.dry_run_speed_mm_s.value(), 88.0f);
    EXPECT_TRUE(stored_launch.runtime_overrides.velocity_trace_enabled);
    EXPECT_EQ(stored_launch.runtime_overrides.velocity_trace_interval_ms, 25);
    EXPECT_EQ(stored_launch.runtime_overrides.velocity_trace_path, "logs/trace.csv");
    EXPECT_TRUE(stored_launch.runtime_overrides.velocity_guard_stop_on_violation);
    EXPECT_TRUE(stored_launch.execution_package);
    EXPECT_TRUE(stored_launch.authority_preview.success);
    EXPECT_FALSE(stored_launch.authority_preview.artifacts.glue_points.empty());
    EXPECT_FALSE(stored_launch.authority_cache_key.empty());
    EXPECT_EQ(prepared.filepath, temp_pb_file.string());
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanUsesCurrentProductionBaselineWithoutLegacyIdentifiers) {
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
    artifact_record.response.artifact_id = "artifact-missing-legacy-identifiers";
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    const auto result = use_case.PreparePlan(BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(result.IsSuccess());
    EXPECT_FALSE(result.Value().plan_fingerprint.empty());
    EXPECT_EQ(result.Value().production_baseline.baseline_id, "test-production-baseline");
    EXPECT_EQ(result.Value().production_baseline.baseline_fingerprint, "baseline-fingerprint");
}

TEST(DispensingWorkflowUseCaseTest, Demo1PreparePlanBlocksFragmentedAndDiscontinuousPathQuality) {
    const auto workspace_root = ResolveWorkspaceRoot();
    ASSERT_FALSE(workspace_root.empty());

    ScopedTempDxfCopy temp_demo_dxf(workspace_root / "samples" / "dxf" / "Demo-1.dxf");
    auto config_port = CreateCanonicalMachineConfigPort(workspace_root);
    const auto loaded_config = config_port->LoadConfiguration();
    ASSERT_TRUE(loaded_config.IsSuccess()) << loaded_config.GetError().ToString();
    ASSERT_GT(loaded_config.Value().dispensing.dot_diameter_target_mm, 0.0f);

    auto planning_use_case = CreatePbBackedPlanningUseCase(config_port);
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto baseline_port = CreateCurrentProductionBaselinePort(config_port);
    auto upload_port = CreateRuntimeLikeUploadPort(config_port);
    auto use_case = CreateUseCaseWithUploadAndPlanning(
        upload_port,
        planning_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port,
        baseline_port,
        config_port);

    const auto artifact_result = use_case.CreateArtifact(BuildUploadRequestFromFile(temp_demo_dxf.path()));
    ASSERT_TRUE(artifact_result.IsSuccess()) << artifact_result.GetError().ToString();

    PreparePlanRequest request;
    request.artifact_id = artifact_result.Value().artifact_id;
    request.planning_request = BuildGovernedDxfGatewayLikePlanningRequest();
    request.runtime_overrides = BuildDemo1ProductionValidationRuntimeOverrides();

    const auto result = use_case.PreparePlan(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    EXPECT_TRUE(result.Value().input_quality.preview_ready);
    EXPECT_FALSE(result.Value().input_quality.production_ready);
    EXPECT_TRUE(result.Value().path_quality.blocking);
    EXPECT_EQ(std::string(Siligen::Shared::Types::ToString(result.Value().path_quality.verdict)), "fail");
    EXPECT_NE(
        std::find(
            result.Value().path_quality.reason_codes.begin(),
            result.Value().path_quality.reason_codes.end(),
            "process_path_fragmentation"),
        result.Value().path_quality.reason_codes.end());
    EXPECT_NE(
        std::find(
            result.Value().path_quality.reason_codes.begin(),
            result.Value().path_quality.reason_codes.end(),
            "path_discontinuity"),
        result.Value().path_quality.reason_codes.end());

    const auto& plan_record = use_case.plans_.at(result.Value().plan_id);
    EXPECT_TRUE(plan_record.execution_contract_ready);
    EXPECT_FALSE(plan_record.execution_assembly.formal_compare_gate.HasValue());
}

TEST(DispensingWorkflowUseCaseTest, GovernedDxfPreparePlanWithProductionLikeInputsReturnsProductionReadyContract) {
    const auto workspace_root = ResolveWorkspaceRoot();
    ASSERT_FALSE(workspace_root.empty());

    ScopedTempDxfCopy temp_dxf(
        workspace_root / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / "rect_diag" / "rect_diag.dxf");
    auto config_port = CreateCanonicalMachineConfigPort(workspace_root);
    const auto loaded_config = config_port->LoadConfiguration();
    ASSERT_TRUE(loaded_config.IsSuccess()) << loaded_config.GetError().ToString();
    ASSERT_GT(loaded_config.Value().dispensing.dot_diameter_target_mm, 0.0f);

    auto planning_use_case = CreatePbBackedPlanningUseCase(config_port);
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto baseline_port = CreateCurrentProductionBaselinePort(config_port);
    auto upload_port = CreateRuntimeLikeUploadPort(config_port);
    auto use_case = CreateUseCaseWithUploadAndPlanning(
        upload_port,
        planning_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port,
        baseline_port,
        config_port);

    const auto artifact_result = use_case.CreateArtifact(BuildUploadRequestFromFile(temp_dxf.path()));
    ASSERT_TRUE(artifact_result.IsSuccess()) << artifact_result.GetError().ToString();

    PreparePlanRequest request;
    request.artifact_id = artifact_result.Value().artifact_id;
    request.planning_request = BuildGovernedDxfGatewayLikePlanningRequest();
    request.runtime_overrides = BuildDemo1ProductionValidationRuntimeOverrides();

    const auto result = use_case.PreparePlan(request);
    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();

    EXPECT_TRUE(result.Value().input_quality.preview_ready);
    EXPECT_TRUE(result.Value().input_quality.production_ready);

    const auto& plan_record = use_case.plans_.at(result.Value().plan_id);
    EXPECT_TRUE(plan_record.execution_contract_ready);
    EXPECT_FALSE(plan_record.execution_assembly.formal_compare_gate.HasValue());
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto first = use_case.PreparePlan(request);
    ASSERT_TRUE(first.IsSuccess()) << first.GetError().ToString();
    auto& stored_record = use_case.plans_.at(first.Value().plan_id);
    stored_record.response.segment_count = 999U;
    stored_record.response.point_count = 999U;
    stored_record.response.total_length_mm = 999.0f;
    stored_record.response.execution_nominal_time_s = 999.0f;

    const auto second = use_case.PreparePlan(request);
    ASSERT_TRUE(second.IsSuccess()) << second.GetError().ToString();
    EXPECT_EQ(first.Value().plan_id, second.Value().plan_id);
    EXPECT_EQ(use_case.plans_.size(), 1U);
    EXPECT_EQ(second.Value().segment_count, first.Value().segment_count);
    EXPECT_EQ(second.Value().point_count, first.Value().point_count);
    EXPECT_FLOAT_EQ(second.Value().total_length_mm, first.Value().total_length_mm);
    EXPECT_FLOAT_EQ(second.Value().execution_nominal_time_s, first.Value().execution_nominal_time_s);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanSingleFlightsConcurrentAuthorityPreviewRequests) {
    ScopedTempPbFile temp_pb_file;
    auto load_calls = std::make_shared<std::atomic<int>>(0);
    auto planning_use_case = CreatePlanningUseCaseWithPathSource(
        std::make_shared<SlowCountingLinePathSourceStub>(
            load_calls,
            std::chrono::milliseconds(500)));
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    auto config_port = std::make_shared<FakeConfigurationPort>();
    ConfigureProfileCompareRuntimeContractConfig(config_port);
    auto planning_use_case = CreatePlanningUseCaseWithPathSourceAndExport(
        std::make_shared<LinePathSourceStub>(),
        std::make_shared<SlowExportPortStub>(export_calls, std::chrono::milliseconds(10)),
        config_port);
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
        interlock_port,
        std::make_shared<FakeProductionBaselinePort>(),
        config_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-start-profile";
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    EXPECT_EQ(response.production_baseline.baseline_id, prepare_result.Value().production_baseline.baseline_id);
    EXPECT_EQ(
        response.production_baseline.baseline_fingerprint,
        prepare_result.Value().production_baseline.baseline_fingerprint);
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    PreparePlanRequest prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);

    const auto prepare_result = use_case.PreparePlan(prepare_request);
    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    const auto plan_id = prepare_result.Value().plan_id;

    const auto& prepared_plan = use_case.plans_.at(plan_id);
    EXPECT_TRUE(prepared_plan.preview_authority_shared_with_execution);
    EXPECT_TRUE(prepared_plan.execution_authority_shared_with_execution);
    EXPECT_TRUE(prepared_plan.execution_binding_ready);
    EXPECT_TRUE(static_cast<bool>(prepared_plan.execution_launch.execution_package));

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = plan_id;
    snapshot_request.max_polyline_points = 128;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();
    const auto& snapshot = snapshot_result.Value();
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
    EXPECT_EQ(snapshot.preview_binding.source, "runtime_authority_preview_binding");
    EXPECT_EQ(snapshot.preview_binding.status, "ready");
    EXPECT_FALSE(snapshot.preview_binding.layout_id.empty());
    EXPECT_EQ(snapshot.preview_binding.glue_point_count, snapshot.glue_point_count);
    EXPECT_EQ(snapshot.preview_binding.source_trigger_indices.size(), snapshot.glue_points.size());
    EXPECT_EQ(snapshot.preview_binding.display_reveal_lengths_mm.size(), snapshot.glue_points.size());
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
    plan_record.response.execution_nominal_time_s = 1.0f;
    plan_record.preview_estimated_time_s = 1.0f;
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
    SeedAuthorityTriggerPoints(plan_record, plan_record.glue_points);
    plan_record.execution_assembly.authority_trigger_layout = plan_record.authority_trigger_layout;
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
    EXPECT_EQ(snapshot.preview_binding.source, "runtime_authority_preview_binding");
    EXPECT_EQ(snapshot.preview_binding.status, "ready");
    EXPECT_EQ(snapshot.preview_binding.binding_basis, "execution_binding_to_motion_preview_polyline");
    ASSERT_EQ(snapshot.preview_binding.source_trigger_indices.size(), 2U);
    EXPECT_EQ(snapshot.preview_binding.source_trigger_indices[0], 0U);
    EXPECT_EQ(snapshot.preview_binding.source_trigger_indices[1], 1U);
    ASSERT_EQ(snapshot.preview_binding.display_reveal_lengths_mm.size(), 2U);
    EXPECT_FLOAT_EQ(snapshot.preview_binding.display_reveal_lengths_mm[0], 0.0f);
    EXPECT_FLOAT_EQ(snapshot.preview_binding.display_reveal_lengths_mm[1], 10.0f);
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

TEST(DispensingWorkflowUseCaseTest, RefreshPathQualityFlagsFragmentationAndDiscontinuityFromAuthorityPreview) {
    auto config_port = std::make_shared<FakeConfigurationPort>();
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-fragmented-discontinuous",
        {
            Point2D(0.0f, 0.0f),
            Point2D(10.0f, 0.0f),
            Point2D(20.0f, 0.0f),
        });
    plan_record.preview_diagnostic_code = "process_path_fragmentation";
    plan_record.execution_launch.authority_preview.discontinuity_count = 1;

    use_case.RefreshPathQualityAssessment(plan_record);

    ASSERT_TRUE(plan_record.path_quality.has_value());
    EXPECT_TRUE(plan_record.path_quality->blocking);
    EXPECT_EQ(std::string(Siligen::Shared::Types::ToString(plan_record.path_quality->verdict)), "fail");
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "process_path_fragmentation"),
        plan_record.path_quality->reason_codes.end());
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "path_discontinuity"),
        plan_record.path_quality->reason_codes.end());
}

TEST(DispensingWorkflowUseCaseTest, RefreshPathQualityFlagsMicroSegmentBurstFromShortSegmentExceptions) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-micro-burst",
        {
            Point2D(0.0f, 0.0f),
            Point2D(2.0f, 0.0f),
            Point2D(4.0f, 0.0f),
            Point2D(6.0f, 0.0f),
            Point2D(8.0f, 0.0f),
        });
    plan_record.preview_validation_classification = "pass_with_exception";
    plan_record.preview_exception_reason = "short_segment_exception";
    plan_record.preview_has_short_segment_exceptions = true;
    plan_record.spacing_validation_groups = {
        BuildSpacingValidationGroup(true, false, 0U, 2.0f),
        BuildSpacingValidationGroup(true, false, 1U, 2.0f),
        BuildSpacingValidationGroup(true, false, 2U, 2.0f),
        BuildSpacingValidationGroup(true, false, 3U, 2.0f),
    };

    use_case.RefreshPathQualityAssessment(plan_record);

    ASSERT_TRUE(plan_record.path_quality.has_value());
    EXPECT_TRUE(plan_record.path_quality->blocking);
    EXPECT_EQ(std::string(Siligen::Shared::Types::ToString(plan_record.path_quality->verdict)), "fail");
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "abnormal_short_segment"),
        plan_record.path_quality->reason_codes.end());
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "micro_segment_burst"),
        plan_record.path_quality->reason_codes.end());
}

TEST(DispensingWorkflowUseCaseTest, RefreshPathQualityFlagsSmallBacktrackFromCanonicalPath) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-small-backtrack",
        {
            Point2D(0.0f, 0.0f),
            Point2D(10.0f, 0.0f),
            Point2D(9.2f, 0.0f),
            Point2D(11.0f, 0.0f),
        });

    use_case.RefreshPathQualityAssessment(plan_record);

    ASSERT_TRUE(plan_record.path_quality.has_value());
    EXPECT_TRUE(plan_record.path_quality->blocking);
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "small_backtrack"),
        plan_record.path_quality->reason_codes.end());
}

TEST(DispensingWorkflowUseCaseTest, RefreshPathQualityBlocksUnallowlistedPassWithException) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-pass-with-exception",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)});
    plan_record.preview_validation_classification = "pass_with_exception";
    plan_record.preview_exception_reason = "future_exception_without_allowlist";

    use_case.RefreshPathQualityAssessment(plan_record);

    ASSERT_TRUE(plan_record.path_quality.has_value());
    EXPECT_TRUE(plan_record.path_quality->blocking);
    EXPECT_EQ(
        std::string(Siligen::Shared::Types::ToString(plan_record.path_quality->verdict)),
        "pass_with_exception");
    EXPECT_NE(
        std::find(
            plan_record.path_quality->reason_codes.begin(),
            plan_record.path_quality->reason_codes.end(),
            "unclassified_path_exception"),
        plan_record.path_quality->reason_codes.end());
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsBlockingPathQualityBeforeCallingExecutionPort) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    auto execution_port = std::make_shared<SpyWorkflowExecutionPort>();
    auto use_case = CreateUseCaseWithPlanningAndExecutionPort(
        MakeDummyShared<PlanningUseCase>(),
        execution_port,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
    SeedPlan(use_case, "plan-start-path-quality-blocked");
    auto& plan_record = use_case.plans_.at("plan-start-path-quality-blocked");
    plan_record.preview_diagnostic_code = "process_path_fragmentation";
    use_case.RefreshPathQualityAssessment(plan_record);

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-start-path-quality-blocked";
    request.plan_fingerprint = "fp-plan-start-path-quality-blocked";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_NE(result.GetError().GetMessage().find("process_path_fragmentation"), std::string::npos);
    EXPECT_EQ(execution_port->start_calls, 0U);
}

TEST(DispensingWorkflowUseCaseTest, StartJobAllowsCleanPathQualityToReachExecutionPort) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;
    auto execution_port = std::make_shared<SpyWorkflowExecutionPort>();
    auto use_case = CreateUseCaseWithPlanningAndExecutionPort(
        MakeDummyShared<PlanningUseCase>(),
        execution_port,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port);
    SeedPlan(use_case, "plan-start-path-quality-clean");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-start-path-quality-clean";
    request.plan_fingerprint = "fp-plan-start-path-quality-clean";
    request.target_count = 2;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(result.Value().started);
    EXPECT_EQ(execution_port->start_calls, 1U);
    EXPECT_EQ(execution_port->last_plan_id, "plan-start-path-quality-clean");
    EXPECT_EQ(execution_port->last_target_count, 2U);
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

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotUsesExecutionTrajectorySnapshotAsMotionPreviewTruth) {
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
    EXPECT_EQ(snapshot.motion_preview_source, "execution_trajectory_snapshot");
    EXPECT_EQ(snapshot.motion_preview_kind, "polyline");
    EXPECT_EQ(snapshot.motion_preview_sampling_strategy, "execution_trajectory_geometry_preserving");
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 0.0f, 0.0f, 1e-4f));
    EXPECT_TRUE(MotionPreviewContainsPoint(snapshot, 50.0f, 0.0f, 1e-4f));
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
    EXPECT_EQ(
        use_case.plans_.at("plan-authority-process-path-fallback").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::FAILED);
    EXPECT_EQ(
        use_case.plans_.at("plan-authority-process-path-fallback").failure_message,
        "motion trajectory snapshot unavailable for preview");
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
    EXPECT_EQ(
        use_case.plans_.at("plan-motion-preview-legacy-fallback").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::FAILED);
    EXPECT_EQ(
        use_case.plans_.at("plan-motion-preview-legacy-fallback").failure_message,
        "motion trajectory snapshot unavailable for preview");
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

TEST(DispensingWorkflowUseCaseTest, GetPreviewSnapshotFailsClosedWhenPreviewBindingPayloadIsUnavailable) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port);

    auto plan_record = BuildPreviewPlanRecord(
        "plan-binding-payload-unavailable",
        {Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f), Point2D(20.0f, 0.0f)});
    plan_record.preview_state = DispensingWorkflowUseCase::PlanPreviewState::PREPARED;
    plan_record.authority_trigger_layout.bindings.pop_back();
    use_case.plans_[plan_record.response.plan_id] = plan_record;

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest request;
    request.plan_id = "plan-binding-payload-unavailable";
    const auto result = use_case.GetPreviewSnapshot(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_EQ(result.GetError().GetMessage(), "preview binding unavailable");
    EXPECT_EQ(
        use_case.plans_.at("plan-binding-payload-unavailable").preview_state,
        DispensingWorkflowUseCase::PlanPreviewState::FAILED);
    EXPECT_EQ(
        use_case.plans_.at("plan-binding-payload-unavailable").failure_message,
        "preview binding unavailable");
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
    auto config_port = std::make_shared<FakeConfigurationPort>();
    ConfigureProfileCompareRuntimeContractConfig(config_port);
    auto planning_use_case = CreateRealPlanningUseCase(config_port);
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
        interlock_port,
        std::make_shared<FakeProductionBaselinePort>(),
        config_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-consistency";
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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

TEST(DispensingWorkflowUseCaseTest, PreparePlanBaselinePolicyUpdatesPlanFingerprint) {
    ScopedTempPbFile temp_pb_file;
    auto planning_use_case = CreateRealPlanningUseCase();
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto baseline_policy_port = std::make_shared<FakeProductionBaselinePort>();
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
        baseline_policy_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-policy-fingerprint";
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    const auto first_prepare = use_case.PreparePlan(
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(first_prepare.IsSuccess()) << first_prepare.GetError().ToString();

    baseline_policy_port->policy.trigger_spatial_interval_mm = 7.5f;
    const auto second_prepare = use_case.PreparePlan(
        BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id));
    ASSERT_TRUE(second_prepare.IsSuccess()) << second_prepare.GetError().ToString();

    EXPECT_NE(second_prepare.Value().plan_fingerprint, first_prepare.Value().plan_fingerprint);
    EXPECT_NE(second_prepare.Value().plan_id, first_prepare.Value().plan_id);
}

TEST(DispensingWorkflowUseCaseTest, ConfirmPreviewRejectsProfileCompareWhenInMotionPositionTriggerCapabilityMissing) {
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
    motion_device_port->capabilities.trigger.supports_in_motion_position_trigger = false;

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest request;
    request.plan_id = "plan-missing-time-trigger";
    request.snapshot_hash = plan_record.preview_snapshot_hash;
    const auto result = use_case.ConfirmPreview(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("in-motion position trigger"), std::string::npos);
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

TEST(DispensingWorkflowUseCaseTest, StartJobAllowsProfileComparePlanRequiringMultipleScheduleSpansBeforeRuntimeLaunch) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-axis-switch-start");

    auto& plan_record = use_case.plans_.at("plan-axis-switch-start");
    ConfigureAxisSwitchProfileComparePlan(plan_record);
    plan_record.response.production_baseline.baseline_id = "baseline-axis-switch";
    plan_record.response.production_baseline.baseline_fingerprint = "baseline-fp-axis-switch";
    plan_record.response.input_quality.report_id = "input-quality-axis-switch";
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-axis-switch-start";
    request.plan_fingerprint = "fp-plan-axis-switch-start";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(plan_record.response.input_quality.preview_ready);
    EXPECT_TRUE(plan_record.response.input_quality.production_ready);
    ASSERT_TRUE(plan_record.execution_launch.profile_compare_schedule);
    ASSERT_TRUE(plan_record.execution_launch.expected_trace);
    EXPECT_EQ(plan_record.execution_launch.profile_compare_schedule->spans.size(), 2U);
    EXPECT_EQ(plan_record.execution_launch.profile_compare_schedule->spans[0].compare_source_axis, 1);
    EXPECT_EQ(plan_record.execution_launch.profile_compare_schedule->spans[0].start_boundary_trigger_count, 1U);
    EXPECT_EQ(plan_record.execution_launch.profile_compare_schedule->spans[1].compare_source_axis, 2);
    EXPECT_EQ(plan_record.execution_launch.profile_compare_schedule->spans[1].start_boundary_trigger_count, 0U);
    EXPECT_EQ(plan_record.authority_trigger_layout.spans.size(), 2U);
    EXPECT_EQ(plan_record.execution_launch.expected_trace->items.size(), 3U);
    EXPECT_EQ(plan_record.execution_launch.expected_trace->items.front().trigger_mode, "start_boundary");
    EXPECT_EQ(plan_record.execution_launch.expected_trace->items.front().authority_span_ref,
        plan_record.authority_trigger_layout.spans[0].span_id);
    EXPECT_EQ(plan_record.execution_launch.expected_trace->items.back().authority_span_ref,
        plan_record.authority_trigger_layout.spans[1].span_id);

    RuntimeJobTraceabilityResponse traceability_response;
    const auto wait_for_traceability = SpinUntil(
        [&]() {
            const auto traceability_result = execution_use_case->GetJobTraceability(result.Value().job_id);
            if (traceability_result.IsError()) {
                return false;
            }
            traceability_response = traceability_result.Value();
            return traceability_response.terminal_state == "completed";
        },
        std::chrono::milliseconds(1000));
    ASSERT_TRUE(wait_for_traceability);
    EXPECT_EQ(traceability_response.verdict, "passed");
    EXPECT_TRUE(traceability_response.strict_one_to_one_proven);
    EXPECT_EQ(traceability_response.production_baseline.baseline_id, "baseline-axis-switch");
    EXPECT_EQ(traceability_response.production_baseline.baseline_fingerprint, "baseline-fp-axis-switch");
    EXPECT_EQ(traceability_response.input_quality.report_id, "input-quality-axis-switch");
    EXPECT_EQ(traceability_response.input_quality.classification, "success");
    EXPECT_TRUE(traceability_response.input_quality.production_ready);
    EXPECT_EQ(traceability_response.expected_trace.size(), 3U);
    EXPECT_EQ(traceability_response.actual_trace.size(), 3U);
    EXPECT_TRUE(traceability_response.mismatches.empty());
    EXPECT_EQ(traceability_response.expected_trace.front().trigger_mode, "start_boundary");
    EXPECT_EQ(traceability_response.expected_trace.back().compare_source_axis, 2);
    EXPECT_EQ(traceability_response.actual_trace.front().compare_source_axis, 1);
    EXPECT_EQ(traceability_response.actual_trace.back().compare_source_axis, 2);
}

TEST(DispensingWorkflowUseCaseTest, StartJobCompilesProfileCompareScheduleFromInterpolationAnchorInsteadOfMotionLeadIn) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-interpolation-anchor-start");

    auto& plan_record = use_case.plans_.at("plan-interpolation-anchor-start");
    ConfigureInterpolationAnchoredProfileComparePlan(plan_record);
    motion_state_port->statuses[LogicalAxisId::X] = ReadyAxisStatus();
    motion_state_port->statuses[LogicalAxisId::Y] = ReadyAxisStatus();
    homing_port->homed[LogicalAxisId::X] = true;
    homing_port->homed[LogicalAxisId::Y] = true;

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-interpolation-anchor-start";
    request.plan_fingerprint = "fp-plan-interpolation-anchor-start";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    ASSERT_TRUE(plan_record.execution_launch.profile_compare_schedule);
    ASSERT_EQ(plan_record.execution_launch.profile_compare_schedule->spans.size(), 1U);
    const auto& span = plan_record.execution_launch.profile_compare_schedule->spans.front();
    EXPECT_EQ(span.compare_source_axis, 1);
    EXPECT_EQ(span.start_boundary_trigger_count, 1U);
    EXPECT_EQ(span.start_position_mm.x, 10.0f);
    EXPECT_EQ(span.start_position_mm.y, 0.0f);
    ASSERT_EQ(span.compare_positions_pulse.size(), 2U);
    EXPECT_GT(span.compare_positions_pulse[0], 0);
    EXPECT_GT(span.compare_positions_pulse[1], span.compare_positions_pulse[0]);
}

TEST(DispensingWorkflowUseCaseTest, StartJobRejectsClosedLoopBranchRevisitProfileComparePlanBeforeRuntimeLaunch) {
    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto execution_use_case =
        CreateRuntimeExecutionUseCase(connection_port, motion_state_port, homing_port, interlock_port);
    auto use_case = CreateUseCase(connection_port, motion_state_port, homing_port, interlock_port, execution_use_case);
    SeedPlan(use_case, "plan-branch-revisit-start");

    auto& plan_record = use_case.plans_.at("plan-branch-revisit-start");
    ConfigureClosedLoopBranchRevisitProfileComparePlan(plan_record);

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-branch-revisit-start";
    request.plan_fingerprint = "fp-plan-branch-revisit-start";
    request.target_count = 1;
    const auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("无法编译正式 execution schedule"), std::string::npos);
    EXPECT_TRUE(plan_record.response.input_quality.preview_ready);
    EXPECT_FALSE(plan_record.response.input_quality.production_ready);
    EXPECT_NE(
        plan_record.response.input_quality.summary.find("owner execution package 不满足 formal runtime compare contract"),
        std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, PreparePlanBuildsProfileCompareOwnerContractForRectDiag) {
    const auto workspace_root = ResolveWorkspaceRoot();
    ASSERT_FALSE(workspace_root.empty());
    const auto rect_diag_pb =
        workspace_root / "shared" / "contracts" / "engineering" / "fixtures" / "cases" /
        "rect_diag" / "rect_diag.pb";
    ASSERT_TRUE(std::filesystem::exists(rect_diag_pb)) << rect_diag_pb.string();

    auto config_port = CreateCanonicalMachineConfigPort(workspace_root);
    auto planning_use_case = CreatePbBackedPlanningUseCase(config_port);

    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = DispensingWorkflowUseCase(
        MakeDummyShared<IUploadFilePort>(),
        planning_use_case,
        std::make_shared<FakeProductionBaselinePort>(),
        MakeDummyShared<Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort>(),
        config_port,
        connection_port,
        std::make_shared<FakeMotionDevicePort>(),
        std::make_shared<FakeDispenserDevicePort>(),
        motion_state_port,
        homing_port,
        interlock_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-owner-gate";
    SeedArtifactSourceDrawing(artifact_record, rect_diag_pb.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    auto request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);
    request.planning_request.dxf_filepath = rect_diag_pb.string();
    const auto result = use_case.PreparePlan(request);

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().ToString();
    EXPECT_TRUE(result.Value().input_quality.preview_ready);
    EXPECT_TRUE(result.Value().input_quality.production_ready);

    const auto& plan_record = use_case.plans_.at(result.Value().plan_id);
    EXPECT_TRUE(plan_record.preview_binding_ready);
    EXPECT_TRUE(plan_record.execution_binding_ready);
    EXPECT_TRUE(plan_record.execution_contract_ready);
    EXPECT_EQ(plan_record.execution_diagnostic_code, "");
    EXPECT_FALSE(plan_record.execution_assembly.formal_compare_gate.HasValue());
}

TEST(DispensingWorkflowUseCaseTest, ProfileCompareOwnerContractKeepsPreviewFlowReadyAfterConfirm) {
    const auto workspace_root = ResolveWorkspaceRoot();
    ASSERT_FALSE(workspace_root.empty());
    const auto rect_diag_pb =
        workspace_root / "shared" / "contracts" / "engineering" / "fixtures" / "cases" /
        "rect_diag" / "rect_diag.pb";
    ASSERT_TRUE(std::filesystem::exists(rect_diag_pb)) << rect_diag_pb.string();

    auto config_port = CreateCanonicalMachineConfigPort(workspace_root);
    auto planning_use_case = CreatePbBackedPlanningUseCase(config_port);

    auto connection_port = std::make_shared<FakeHardwareConnectionPort>();
    auto motion_state_port = std::make_shared<FakeMotionStatePort>();
    auto homing_port = std::make_shared<FakeHomingPort>();
    auto interlock_port = std::make_shared<FakeInterlockSignalPort>();
    auto use_case = CreateUseCaseWithPlanning(
        planning_use_case,
        connection_port,
        motion_state_port,
        homing_port,
        interlock_port,
        std::make_shared<FakeProductionBaselinePort>(),
        config_port);

    DispensingWorkflowUseCase::ArtifactRecord artifact_record;
    artifact_record.response.artifact_id = "artifact-owner-gate-preview";
    SeedArtifactSourceDrawing(artifact_record, rect_diag_pb.string());
    use_case.artifacts_[artifact_record.response.artifact_id] = artifact_record;

    auto prepare_request = BuildCanonicalPreparePlanRequest(artifact_record.response.artifact_id);
    prepare_request.planning_request.dxf_filepath = rect_diag_pb.string();
    const auto prepare_result = use_case.PreparePlan(prepare_request);

    ASSERT_TRUE(prepare_result.IsSuccess()) << prepare_result.GetError().ToString();
    EXPECT_TRUE(prepare_result.Value().input_quality.preview_ready);
    EXPECT_TRUE(prepare_result.Value().input_quality.production_ready);

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = prepare_result.Value().plan_id;
    const auto snapshot_result = use_case.GetPreviewSnapshot(snapshot_request);

    ASSERT_TRUE(snapshot_result.IsSuccess()) << snapshot_result.GetError().ToString();
    EXPECT_EQ(snapshot_result.Value().preview_state, "snapshot_ready");

    Siligen::Application::UseCases::Dispensing::ConfirmPreviewRequest confirm_request;
    confirm_request.plan_id = prepare_result.Value().plan_id;
    confirm_request.snapshot_hash = snapshot_result.Value().snapshot_hash;
    const auto confirm_result = use_case.ConfirmPreview(confirm_request);

    ASSERT_TRUE(confirm_result.IsSuccess()) << confirm_result.GetError().ToString();
    EXPECT_EQ(confirm_result.Value().preview_state, "confirmed");

    const auto& plan_record = use_case.plans_.at(prepare_result.Value().plan_id);
    EXPECT_EQ(plan_record.preview_state, DispensingWorkflowUseCase::PlanPreviewState::CONFIRMED);
    EXPECT_TRUE(plan_record.response.input_quality.preview_ready);
    EXPECT_TRUE(plan_record.response.input_quality.production_ready);
    EXPECT_EQ(plan_record.preview_failure_reason, prepare_result.Value().preview_failure_reason);
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
    plan_record.execution_contract_ready = false;
    plan_record.execution_failure_reason = "authority trigger binding unavailable";
    plan_record.execution_assembly.execution_contract_ready = false;
    plan_record.execution_assembly.execution_failure_reason = "authority trigger binding unavailable";

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-binding-mismatch";
    request.plan_fingerprint = "fp-plan-binding-mismatch";
    request.target_count = 1;
    auto result = use_case.StartJob(request);

    ASSERT_TRUE(result.IsError());
    EXPECT_EQ(result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(result.GetError().GetMessage().find("binding"), std::string::npos);
}

TEST(DispensingWorkflowUseCaseTest, StartJobWaitsForExplicitContinueBetweenCycles) {
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
    SeedPlan(use_case, "plan-wait-continue");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-wait-continue";
    request.plan_fingerprint = "fp-plan-wait-continue";
    request.target_count = 2;
    request.cycle_advance_mode = JobCycleAdvanceMode::WAIT_FOR_CONTINUE;

    const auto start_result = use_case.StartJob(request);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().ToString();

    RuntimeJobStatusResponse waiting_status;
    const auto wait_for_continue = SpinUntil(
        [&]() {
            const auto status_result = execution_use_case->GetJobStatus(start_result.Value().job_id);
            if (status_result.IsError()) {
                return false;
            }
            waiting_status = status_result.Value();
            return waiting_status.state == "awaiting_continue";
        },
        std::chrono::milliseconds(1000));
    ASSERT_TRUE(wait_for_continue);
    EXPECT_EQ(waiting_status.transition_state, Siligen::Application::UseCases::Dispensing::ExecutionTransitionState::AWAITING_CONTINUE);
    EXPECT_EQ(waiting_status.completed_count, 1U);
    EXPECT_EQ(waiting_status.current_cycle, 1U);

    const auto pause_result = execution_use_case->PauseJob(start_result.Value().job_id);
    ASSERT_TRUE(pause_result.IsError());
    EXPECT_EQ(pause_result.GetError().GetCode(), ErrorCode::INVALID_STATE);

    const auto resume_result = execution_use_case->ResumeJob(start_result.Value().job_id);
    ASSERT_TRUE(resume_result.IsError());
    EXPECT_EQ(resume_result.GetError().GetCode(), ErrorCode::INVALID_STATE);

    const auto continue_result = execution_use_case->ContinueJob(start_result.Value().job_id);
    ASSERT_TRUE(continue_result.IsSuccess()) << continue_result.GetError().ToString();

    RuntimeJobStatusResponse completed_status;
    const auto wait_for_completion = SpinUntil(
        [&]() {
            const auto status_result = execution_use_case->GetJobStatus(start_result.Value().job_id);
            if (status_result.IsError()) {
                return false;
            }
            completed_status = status_result.Value();
            return completed_status.state == "completed";
        },
        std::chrono::milliseconds(1000));
    ASSERT_TRUE(wait_for_completion);
    EXPECT_EQ(completed_status.completed_count, 2U);
    EXPECT_EQ(completed_status.target_count, 2U);
}

TEST(DispensingWorkflowUseCaseTest, StartJobAutoContinueCompletesMultiCycleJobWithoutManualContinue) {
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
    SeedPlan(use_case, "plan-auto-continue");

    Siligen::Application::UseCases::Dispensing::StartJobRequest request;
    request.plan_id = "plan-auto-continue";
    request.plan_fingerprint = "fp-plan-auto-continue";
    request.target_count = 2;
    request.cycle_advance_mode = JobCycleAdvanceMode::AUTO_CONTINUE;

    const auto start_result = use_case.StartJob(request);
    ASSERT_TRUE(start_result.IsSuccess()) << start_result.GetError().ToString();

    RuntimeJobStatusResponse completed_status;
    const auto wait_for_completion = SpinUntil(
        [&]() {
            const auto status_result = execution_use_case->GetJobStatus(start_result.Value().job_id);
            if (status_result.IsError()) {
                return false;
            }
            completed_status = status_result.Value();
            return completed_status.state == "completed";
        },
        std::chrono::milliseconds(1000));
    ASSERT_TRUE(wait_for_completion);
    EXPECT_EQ(completed_status.completed_count, 2U);
    EXPECT_EQ(completed_status.target_count, 2U);
}

TEST(DispensingWorkflowUseCaseTest, ContinueJobRejectsNonWaitingState) {
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

    RuntimeJobStatusResponse runtime_status;
    runtime_status.job_id = "job-non-waiting";
    runtime_status.plan_id = "plan-non-waiting";
    runtime_status.plan_fingerprint = "fp-plan-non-waiting";
    runtime_status.state = "running";
    runtime_status.target_count = 2;
    runtime_status.current_cycle = 1;
    runtime_status.dry_run = true;
    execution_use_case->SeedJobStateForTesting(runtime_status);
    execution_use_case->SetActiveJobForTesting(runtime_status.job_id);

    const auto continue_result = execution_use_case->ContinueJob(runtime_status.job_id);

    ASSERT_TRUE(continue_result.IsError());
    EXPECT_EQ(continue_result.GetError().GetCode(), ErrorCode::INVALID_STATE);
    EXPECT_NE(continue_result.GetError().GetMessage().find("not waiting for continue"), std::string::npos);
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
    plan_record.execution_contract_ready = true;
    plan_record.execution_failure_reason = "stale execution failure";
    plan_record.execution_diagnostic_code = "stale_execution_diagnostic";
    plan_record.profile_compare_schedule_failure = "stale profile compare schedule";
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
    plan_record.execution_launch.profile_compare_schedule =
        std::make_shared<ProfileCompareExecutionSchedule>();
    auto expected_trace =
        std::make_shared<Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTrace>();
    expected_trace->items.push_back(
        Siligen::Domain::Dispensing::ValueObjects::ProfileCompareExpectedTraceItem{});
    plan_record.execution_launch.expected_trace = expected_trace;
    SeedAuthorityMetadata(plan_record, "layout-plan-finish");
    plan_record.execution_assembly.success = true;
    plan_record.execution_assembly.execution_trajectory_points = plan_record.execution_trajectory_points;
    plan_record.execution_assembly.preview_authority_shared_with_execution = true;
    plan_record.execution_assembly.execution_binding_ready = true;
    plan_record.execution_assembly.execution_contract_ready = true;
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
    EXPECT_FALSE(use_case.plans_.at("plan-finish").execution_contract_ready);
    EXPECT_TRUE(use_case.plans_.at("plan-finish").execution_failure_reason.empty());
    EXPECT_TRUE(use_case.plans_.at("plan-finish").execution_diagnostic_code.empty());
    EXPECT_FALSE(static_cast<bool>(use_case.plans_.at("plan-finish").execution_launch.profile_compare_schedule));
    EXPECT_FALSE(static_cast<bool>(use_case.plans_.at("plan-finish").execution_launch.expected_trace));
    EXPECT_TRUE(use_case.plans_.at("plan-finish").profile_compare_schedule_failure.empty());
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
    SeedArtifactSourceDrawing(artifact_record, temp_pb_file.string());
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
    EXPECT_FALSE(use_case.plans_.at(plan_id).retained_preview_motion_trajectory_points.empty());
    EXPECT_TRUE(use_case.execution_assembly_cache_.find(plan_fingerprint) == use_case.execution_assembly_cache_.end());

    Siligen::Application::UseCases::Dispensing::PreviewSnapshotRequest snapshot_request;
    snapshot_request.plan_id = plan_id;
    snapshot_request.max_polyline_points = 128;
    const auto readback_result = use_case.GetPreviewSnapshot(snapshot_request);
    ASSERT_TRUE(readback_result.IsSuccess()) << readback_result.GetError().ToString();
    EXPECT_EQ(readback_result.Value().plan_id, plan_id);
    EXPECT_EQ(readback_result.Value().snapshot_hash, plan_fingerprint);
    EXPECT_GT(readback_result.Value().motion_preview_point_count, 0U);
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_launch.execution_package);
    EXPECT_FALSE(use_case.plans_.at(plan_id).execution_assembly.execution_package);
    EXPECT_TRUE(use_case.plans_.at(plan_id).execution_assembly.motion_trajectory_points.empty());

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

