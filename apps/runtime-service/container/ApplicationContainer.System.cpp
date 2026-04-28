#include "ApplicationContainer.h"

#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "runtime_execution/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/system/EmergencyStopUseCase.h"
#include "runtime_execution/application/usecases/system/InitializeSystemUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "services/motion/SoftLimitMonitorService.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "runtime/status/RuntimeStatusExportPort.h"
#include "runtime/supervision/RuntimeExecutionSupervisionBackend.h"
#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "runtime/supervision/RuntimeJobTerminalSync.h"
#include "runtime/supervision/RuntimeSupervisionSyncPort.h"
#include "runtime/system/MachineExecutionStatePortFactory.h"
#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"
#include "runtime_execution/application/services/motion/MotionStatusServiceImpl.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "runtime_execution/contracts/system/IRuntimeStatusExportPort.h"
#include "runtime_execution/contracts/system/IRuntimeSupervisionPort.h"
#include "shared/interfaces/ILoggingService.h"

#include <memory>
#include <stdexcept>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.System"

namespace Siligen::Application::Container {

void ApplicationContainer::ValidateSystemPorts() {
    if (!config_port_) {
        throw std::runtime_error("IConfigurationPort 未注册");
    }
    if (!device_connection_port_) {
        throw std::runtime_error("DeviceConnectionPort 未注册");
    }
    if (!event_port_) {
        SILIGEN_LOG_WARNING("IEventPublisherPort 未注册");
    }
}

void ApplicationContainer::ValidateDiagnosticsPorts() {
    if (!diagnostics_port_) {
        throw std::runtime_error("IDiagnosticsPort 未注册");
    }
    if (!test_record_repository_) {
        SILIGEN_LOG_WARNING("ITestRecordRepository 未注册");
    }
    if (!test_config_manager_) {
        SILIGEN_LOG_WARNING("ITestConfigurationPort 未注册");
    }
    if (!preset_port_) {
        SILIGEN_LOG_WARNING("ICMPTestPresetPort 未注册");
    }
}

void ApplicationContainer::ConfigureSystemOwnerPorts() {
    auto dispensing_execution_use_case = Resolve<UseCases::Dispensing::DispensingExecutionUseCase>();
    if (auto soft_limit_monitor =
            std::dynamic_pointer_cast<Siligen::Application::Services::Motion::SoftLimitMonitorService>(
                soft_limit_monitor_);
        soft_limit_monitor && dispensing_execution_use_case) {
        soft_limit_monitor->SetActiveJobIdProvider([dispensing_execution_use_case]() {
            return dispensing_execution_use_case->GetActiveJobId();
        });
    }

    auto runtime_supervision_port =
        ResolvePort<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>();
    if (!runtime_supervision_port) {
        auto terminal_job_sync =
            std::make_shared<Siligen::Runtime::Service::Supervision::RuntimeJobTerminalSync>(
                Resolve<UseCases::Dispensing::DispensingWorkflowUseCase>());
        auto backend = std::make_shared<Siligen::Runtime::Service::Supervision::RuntimeExecutionSupervisionBackend>(
            Resolve<UseCases::Motion::MotionControlUseCase>(),
            Resolve<UseCases::System::EmergencyStopUseCase>(),
            dispensing_execution_use_case,
            ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>(),
            device_connection_port_);
        runtime_supervision_port = std::make_shared<Siligen::Runtime::Service::Supervision::RuntimeSupervisionSyncPort>(
            std::make_shared<Siligen::Runtime::Host::Supervision::RuntimeSupervisionPortAdapter>(backend),
            dispensing_execution_use_case,
            terminal_job_sync);
        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>(runtime_supervision_port);
    }

    if (!ResolvePort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>()) {
        Siligen::Runtime::Service::Status::RuntimeMotionStatusReader motion_status_reader;
        if (auto monitoring_use_case = Resolve<UseCases::Motion::Monitoring::MotionMonitoringUseCase>()) {
            motion_status_reader.read_all_axes_motion_status = [monitoring_use_case]() {
                return monitoring_use_case->GetAllAxesMotionStatus();
            };
            motion_status_reader.read_current_position = [monitoring_use_case]() {
                return monitoring_use_case->GetCurrentPosition();
            };
        }

        Siligen::Runtime::Service::Status::RuntimeDispenserStatusReader dispenser_status_reader;
        if (auto valve_query_use_case = Resolve<UseCases::Dispensing::Valve::ValveQueryUseCase>()) {
            dispenser_status_reader.read_dispenser_status = [valve_query_use_case]() {
                return valve_query_use_case->GetDispenserStatus();
            };
            dispenser_status_reader.read_supply_status = [valve_query_use_case]() {
                return valve_query_use_case->GetSupplyStatus();
            };
        }

        Siligen::Runtime::Service::Status::RuntimeJobExecutionStatusReader job_execution_status_reader;
        if (auto dispensing_execution_use_case = Resolve<UseCases::Dispensing::DispensingExecutionUseCase>()) {
            job_execution_status_reader.read_job_status = [dispensing_execution_use_case](const std::string& job_id) {
                auto runtime_result = dispensing_execution_use_case->GetJobStatus(job_id);
                if (runtime_result.IsError()) {
                    return Siligen::Shared::Types::Result<
                        Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot>::Failure(
                        runtime_result.GetError());
                }

                const auto& runtime_status = runtime_result.Value();
                Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot snapshot;
                snapshot.job_id = runtime_status.job_id;
                snapshot.plan_id = runtime_status.plan_id;
                snapshot.plan_fingerprint = runtime_status.plan_fingerprint;
                snapshot.state = runtime_status.state;
                snapshot.target_count = runtime_status.target_count;
                snapshot.completed_count = runtime_status.completed_count;
                snapshot.current_cycle = runtime_status.current_cycle;
                snapshot.current_segment = runtime_status.current_segment;
                snapshot.total_segments = runtime_status.total_segments;
                snapshot.cycle_progress_percent = runtime_status.cycle_progress_percent;
                snapshot.overall_progress_percent = runtime_status.overall_progress_percent;
                snapshot.elapsed_seconds = runtime_status.elapsed_seconds;
                snapshot.error_message = runtime_status.error_message;
                snapshot.dry_run = runtime_status.dry_run;
                return Siligen::Shared::Types::Result<
                    Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot>::Success(
                    std::move(snapshot));
            };
        }

        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>(
            std::make_shared<Siligen::Runtime::Service::Status::RuntimeStatusExportPort>(
                runtime_supervision_port,
                std::move(motion_status_reader),
                std::move(dispenser_status_reader),
                std::move(job_execution_status_reader)));
    }
}

template<>
std::shared_ptr<UseCases::System::InitializeSystemUseCase>
ApplicationContainer::CreateInstance<UseCases::System::InitializeSystemUseCase>() {
    auto home_axes_usecase = Resolve<UseCases::Motion::Homing::HomeAxesUseCase>();
    return std::make_shared<UseCases::System::InitializeSystemUseCase>(
        config_port_,
        device_connection_port_,
        home_axes_usecase,
        diagnostics_port_,
        event_port_,
        hard_limit_monitor_,
        soft_limit_monitor_);
}

template<>
std::shared_ptr<UseCases::System::EmergencyStopUseCase>
ApplicationContainer::CreateInstance<UseCases::System::EmergencyStopUseCase>() {
    if (!machine_execution_state_port_) {
        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>(
            Siligen::RuntimeExecution::Host::System::CreateMachineExecutionStatePort());
    }
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto motion_control_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl>(
            position_control_port);
    auto motion_status_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl>(
            motion_state_port);

    return std::make_shared<UseCases::System::EmergencyStopUseCase>(
        motion_control_service,
        motion_status_service,
        trigger_port_,
        machine_execution_state_port_,
        logging_service_);
}

}  // namespace Siligen::Application::Container
