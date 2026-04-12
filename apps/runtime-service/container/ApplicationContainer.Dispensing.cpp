#include "ApplicationContainer.h"

#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "workflow/application/ports/dispensing/PlanningPortAdapters.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "job_ingest/application/ports/dispensing/UploadPorts.h"
#include "job_ingest/application/usecases/dispensing/UploadFileUseCase.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "runtime_process_bootstrap/storage/ports/IFileStoragePort.h"
#include "runtime_execution/application/services/dispensing/DispensingProcessPortFactory.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime/planning/PlanningArtifactExportPortAdapter.h"
#include "workflow/application/phase-control/DispensingWorkflowUseCase.h"
#include "workflow/application/planning-trigger/PlanningUseCase.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <memory>
#include <stdexcept>
#include <utility>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Dispensing"

namespace {

using Siligen::Application::Ports::Dispensing::IWorkflowExecutionPort;
using Siligen::Application::Ports::Dispensing::WorkflowJobId;
using Siligen::Application::Ports::Dispensing::WorkflowRuntimeStartJobRequest;
using Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase;
using Siligen::Application::UseCases::Dispensing::RuntimeStartJobRequest;
using Siligen::Application::Services::DXF::DxfPbPreparationService;
using Siligen::Domain::Configuration::Ports::FileData;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Configuration::Ports::IFileStoragePort;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadPreparationPort;
using Siligen::JobIngest::Application::Ports::Dispensing::IUploadStoragePort;
using Siligen::JobIngest::Application::UseCases::Dispensing::UploadFileUseCase;
using Siligen::JobIngest::Contracts::IUploadFilePort;
using Siligen::JobIngest::Contracts::UploadRequest;
using Siligen::Shared::Types::Result;

class RuntimeWorkflowExecutionPortAdapter final : public IWorkflowExecutionPort {
   public:
    explicit RuntimeWorkflowExecutionPortAdapter(std::shared_ptr<DispensingExecutionUseCase> use_case)
        : use_case_(std::move(use_case)) {
        if (!use_case_) {
            throw std::invalid_argument("RuntimeWorkflowExecutionPortAdapter: use_case cannot be null");
        }
    }

    Result<WorkflowJobId> StartJob(const WorkflowRuntimeStartJobRequest& request) override {
        RuntimeStartJobRequest runtime_request;
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

class RuntimeUploadStorageAdapter final : public IUploadStoragePort {
   public:
    explicit RuntimeUploadStorageAdapter(std::shared_ptr<IFileStoragePort> file_storage_port)
        : file_storage_port_(std::move(file_storage_port)) {
        if (!file_storage_port_) {
            throw std::invalid_argument("RuntimeUploadStorageAdapter: file_storage_port cannot be null");
        }
    }

    Result<void> Validate(const UploadRequest& request,
                          size_t max_file_size_mb,
                          const std::vector<std::string>& allowed_extensions) const override {
        FileData file_data{request.file_content, request.original_filename, request.file_size, request.content_type};
        return file_storage_port_->ValidateFile(file_data, max_file_size_mb, allowed_extensions);
    }

    Result<std::string> Store(const UploadRequest& request, const std::string& target_filename) override {
        FileData file_data{request.file_content, request.original_filename, request.file_size, request.content_type};
        return file_storage_port_->StoreFile(file_data, target_filename);
    }

    Result<void> Delete(const std::string& stored_path) override {
        return file_storage_port_->DeleteFile(stored_path);
    }

   private:
    std::shared_ptr<IFileStoragePort> file_storage_port_;
};

class DxfPreparationAdapter final : public IUploadPreparationPort {
   public:
    explicit DxfPreparationAdapter(std::shared_ptr<IConfigurationPort> config_port)
        : service_(std::move(config_port)) {}

    Result<std::string> EnsurePreparedInput(const std::string& source_path) const override {
        return service_.EnsurePbReady(source_path);
    }

    Result<void> CleanupPreparedInput(const std::string& source_path) const override {
        return service_.CleanupPreparedInput(source_path);
    }

   private:
    DxfPbPreparationService service_;
};

std::shared_ptr<IWorkflowExecutionPort> CreateRuntimeWorkflowExecutionPort(
    std::shared_ptr<DispensingExecutionUseCase> use_case) {
    return std::make_shared<RuntimeWorkflowExecutionPortAdapter>(std::move(use_case));
}

}  // namespace

namespace Siligen::Application::Container {

void ApplicationContainer::ValidateDispensingPorts() {
    if (!trigger_port_) {
        throw std::runtime_error("ITriggerControllerPort 未注册");
    }
    if (!valve_port_) {
        throw std::runtime_error("IValvePort 未注册");
    }
    if (!file_storage_port_) {
        throw std::runtime_error("runtime bootstrap IFileStoragePort 未注册");
    }
    if (!task_scheduler_port_) {
        SILIGEN_LOG_WARNING("ITaskSchedulerPort 未注册");
    }
}

void ApplicationContainer::ConfigureDispensingServices() {
    SILIGEN_LOG_INFO("Dispensing services configuration complete");
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveCommandUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveCommandUseCase>(
        valve_port_,
        config_port_,
        device_connection_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveQueryUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveQueryUseCase>(valve_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::PlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::PlanningUseCase>() {
    auto path_source = ResolvePort<ProcessPath::Contracts::IPathSourcePort>();
    if (!path_source) {
        throw std::runtime_error("IPathSourcePort 未注册");
    }

    auto planning_operations =
        Siligen::Application::Services::Dispensing::WorkflowPlanningAssemblyOperationsProvider{}
            .CreateOperations();
    auto pb_preparation_service =
        std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(config_port_);

    return std::make_shared<UseCases::Dispensing::PlanningUseCase>(
        path_source,
        Siligen::Application::Ports::Dispensing::AdaptProcessPathFacade(
            std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>()),
        Siligen::Application::Ports::Dispensing::AdaptMotionPlanningFacade(
            std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(velocity_profile_port_)),
        std::move(planning_operations),
        config_port_,
        Siligen::Application::Ports::Dispensing::AdaptDxfPreparationService(std::move(pb_preparation_service)),
        Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort(),
        diagnostics_port_,
        event_port_);
}

template<>
std::shared_ptr<UploadFileUseCase>
ApplicationContainer::CreateInstance<UploadFileUseCase>() {
    return std::make_shared<UploadFileUseCase>(
        std::make_shared<RuntimeUploadStorageAdapter>(file_storage_port_),
        std::make_shared<DxfPreparationAdapter>(config_port_),
        10);
}

template<>
std::shared_ptr<IUploadFilePort>
ApplicationContainer::CreateInstance<IUploadFilePort>() {
    return std::static_pointer_cast<IUploadFilePort>(
        Resolve<UploadFileUseCase>());
}

template<>
std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingExecutionUseCase>() {
    auto process_port =
        Siligen::RuntimeExecution::Application::Services::Dispensing::CreateDispensingProcessPort(
            valve_port_,
            interpolation_port_,
            motion_state_port_,
            device_connection_port_,
            config_port_);
    return std::make_shared<UseCases::Dispensing::DispensingExecutionUseCase>(
        valve_port_,
        interpolation_port_,
        motion_state_port_,
        device_connection_port_,
        config_port_,
        process_port,
        event_port_,
        task_scheduler_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

template<>
std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingWorkflowUseCase>() {
    return std::make_shared<UseCases::Dispensing::DispensingWorkflowUseCase>(
        Resolve<IUploadFilePort>(),
        Resolve<UseCases::Dispensing::PlanningUseCase>(),
        CreateRuntimeWorkflowExecutionPort(
            Resolve<UseCases::Dispensing::DispensingExecutionUseCase>()),
        device_connection_port_,
        motion_state_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

}  // namespace Siligen::Application::Container


