#include "ApplicationContainer.h"

#include "dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
#include "application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/ports/dispensing/PlanningPortAdapters.h"
#include "application/ports/dispensing/WorkflowExecutionPortAdapters.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "process_path/contracts/IPathSourcePort.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "runtime_execution/application/services/dispensing/DispensingProcessPortFactory.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime/planning/PlanningArtifactExportPortAdapter.h"
#include "workflow/application/phase-control/DispensingWorkflowUseCase.h"
#include "workflow/application/planning-trigger/PlanningUseCase.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Result.h"

#include <memory>
#include <stdexcept>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Dispensing"

namespace Siligen::Application::Container {

void ApplicationContainer::ValidateDispensingPorts() {
    if (!trigger_port_) {
        throw std::runtime_error("ITriggerControllerPort 未注册");
    }
    if (!valve_port_) {
        throw std::runtime_error("IValvePort 未注册");
    }
    if (!file_storage_port_) {
        throw std::runtime_error("IFileStoragePort 未注册");
    }
    if (!task_scheduler_port_) {
        SILIGEN_LOG_WARNING("ITaskSchedulerPort 未注册");
    }
}

void ApplicationContainer::ConfigureDispensingServices() {
    valve_controller_ =
        std::make_shared<Domain::Dispensing::DomainServices::ValveCoordinationService>(valve_port_);
    RegisterService<Domain::Dispensing::DomainServices::ValveCoordinationService>(valve_controller_);
    SILIGEN_LOG_INFO("ValveController registered");
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveCommandUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveCommandUseCase>(
        valve_controller_,
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
    auto path_source = ResolvePort<Domain::Trajectory::Ports::IPathSourcePort>();
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
std::shared_ptr<UseCases::Dispensing::UploadFileUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::UploadFileUseCase>() {
    return std::make_shared<UseCases::Dispensing::UploadFileUseCase>(
        file_storage_port_,
        10,
        config_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::IUploadFilePort>
ApplicationContainer::CreateInstance<UseCases::Dispensing::IUploadFilePort>() {
    return std::static_pointer_cast<UseCases::Dispensing::IUploadFilePort>(
        Resolve<UseCases::Dispensing::UploadFileUseCase>());
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
        Resolve<UseCases::Dispensing::IUploadFilePort>(),
        Resolve<UseCases::Dispensing::PlanningUseCase>(),
        Siligen::Application::Ports::Dispensing::AdaptRuntimeDispensingExecutionUseCase(
            Resolve<UseCases::Dispensing::DispensingExecutionUseCase>()),
        device_connection_port_,
        motion_state_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

}  // namespace Siligen::Application::Container


