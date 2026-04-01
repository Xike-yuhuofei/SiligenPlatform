#include "ApplicationContainer.h"

#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/system/EmergencyStopUseCase.h"
#include "application/usecases/system/InitializeSystemUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "runtime/status/WorkflowRuntimeStatusExportPort.h"
#include "runtime/supervision/WorkflowRuntimeSupervisionBackend.h"
#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "runtime/system/DispenserModelMachineExecutionStateBackend.h"
#include "runtime/system/LegacyMachineExecutionStateAdapter.h"
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
        auto backend = std::make_shared<Siligen::Runtime::Service::Supervision::WorkflowRuntimeSupervisionBackend>(
            Resolve<UseCases::Motion::MotionControlUseCase>(),
            Resolve<UseCases::System::EmergencyStopUseCase>(),
            Resolve<UseCases::Dispensing::DispensingWorkflowUseCase>(),
            Resolve<UseCases::Dispensing::DispensingExecutionUseCase>(),
            device_connection_port_);
        runtime_supervision_port =
            std::make_shared<Siligen::Runtime::Host::Supervision::RuntimeSupervisionPortAdapter>(backend);
        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort>(runtime_supervision_port);
    }

    if (!ResolvePort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>()) {
        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IRuntimeStatusExportPort>(
            std::make_shared<Siligen::Runtime::Service::Status::WorkflowRuntimeStatusExportPort>(
                runtime_supervision_port,
                Resolve<UseCases::Motion::MotionControlUseCase>(),
                Resolve<UseCases::Dispensing::Valve::ValveQueryUseCase>()));
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
        auto backend = std::make_shared<Siligen::Runtime::Service::System::DispenserModelMachineExecutionStateBackend>();
        RegisterPort<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>(
            std::make_shared<Siligen::Runtime::Host::System::LegacyMachineExecutionStateAdapter>(backend));
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
