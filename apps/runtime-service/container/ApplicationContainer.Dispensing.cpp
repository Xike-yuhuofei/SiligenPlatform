#include "ApplicationContainer.h"

#include "application/services/dispensing/DispensePlanningFacade.h"
#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "application/usecases/dispensing/CleanupFilesUseCase.h"
#include "application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "domain/trajectory/ports/IPathSourcePort.h"
#include "application/usecases/dispensing/UploadFileUseCase.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime/planning/PlanningArtifactExportPortAdapter.h"
#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "application/usecases/dispensing/PlanningUseCase.h"
#include "shared/interfaces/ILoggingService.h"

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

    return std::make_shared<UseCases::Dispensing::PlanningUseCase>(
        path_source,
        std::make_shared<Siligen::Application::Services::ProcessPath::ProcessPathFacade>(),
        std::make_shared<Siligen::Application::Services::MotionPlanning::MotionPlanningFacade>(
            velocity_profile_service_),
        std::make_shared<Siligen::Application::Services::Dispensing::DispensePlanningFacade>(),
        config_port_,
        nullptr,
        std::make_shared<Siligen::RuntimeExecution::Host::Planning::PlanningArtifactExportPortAdapter>());
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
std::shared_ptr<UseCases::Dispensing::CleanupFilesUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::CleanupFilesUseCase>() {
    return std::make_shared<UseCases::Dispensing::CleanupFilesUseCase>(
        file_storage_port_,
        upload_base_dir_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DispensingExecutionUseCase>() {
    return std::make_shared<UseCases::Dispensing::DispensingExecutionUseCase>(
        valve_port_,
        interpolation_port_,
        motion_state_port_,
        device_connection_port_,
        config_port_,
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
        Resolve<UseCases::Dispensing::DispensingExecutionUseCase>(),
        device_connection_port_,
        motion_state_port_,
        homing_port_,
        ResolvePort<Domain::Safety::Ports::IInterlockSignalPort>());
}

}  // namespace Siligen::Application::Container


