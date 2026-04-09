#include "ApplicationContainer.h"

#include "workflow/application/phase-control/DispensingWorkflowUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/system/EmergencyStopUseCase.h"
#include "runtime_execution/application/usecases/system/InitializeSystemUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
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
    auto runtime_supervision_port =
        ResolvePort<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>();
    if (!runtime_supervision_port) {
        auto dispensing_execution_use_case = Resolve<UseCases::Dispensing::DispensingExecutionUseCase>();
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

        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>(
            std::make_shared<Siligen::Runtime::Service::Status::RuntimeStatusExportPort>(
                runtime_supervision_port,
                std::move(motion_status_reader),
                std::move(dispenser_status_reader)));
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
        hard_limit_monitor_);
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
    auto cmp_service = std::make_shared<Domain::Dispensing::DomainServices::CMPService>(
        trigger_port_,
        logging_service_);
    auto motion_control_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl>(
            position_control_port);
    auto motion_status_service =
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl>(
            motion_state_port);

    return std::make_shared<UseCases::System::EmergencyStopUseCase>(
        motion_control_service,
        motion_status_service,
        cmp_service,
        machine_execution_state_port_,
        logging_service_);
}

}  // namespace Siligen::Application::Container
