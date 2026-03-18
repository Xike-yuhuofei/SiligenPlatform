#include "ApplicationContainer.h"

#include "application/usecases/dispensing/dxf/CleanupDXFFilesUseCase.h"
#include "application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.h"
#include "application/usecases/dispensing/dxf/DXFDispensingPlanner.h"
#include "application/usecases/dispensing/dxf/DXFWebPlanningUseCase.h"
#include "application/usecases/dispensing/dxf/UploadDXFFileUseCase.h"
#include "application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
#include "domain/trajectory/ports/IPathSourcePort.h"
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
        hardware_connection_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::Valve::ValveQueryUseCase>() {
    return std::make_shared<UseCases::Dispensing::Valve::ValveQueryUseCase>(valve_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DXF::DXFWebPlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DXF::DXFWebPlanningUseCase>() {
    auto path_source = ResolvePort<Domain::Trajectory::Ports::IPathSourcePort>();
    if (!path_source) {
        throw std::runtime_error("IPathSourcePort 未注册");
    }

    auto interpolation_usecase = Resolve<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>();
    auto planner = std::make_shared<UseCases::Dispensing::DXF::DXFDispensingPlanner>(
        path_source,
        interpolation_usecase,
        velocity_profile_service_);

    return std::make_shared<UseCases::Dispensing::DXF::DXFWebPlanningUseCase>(
        planner,
        config_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DXF::UploadDXFFileUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DXF::UploadDXFFileUseCase>() {
    return std::make_shared<UseCases::Dispensing::DXF::UploadDXFFileUseCase>(
        file_storage_port_,
        10,
        config_port_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DXF::CleanupDXFFilesUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DXF::CleanupDXFFilesUseCase>() {
    return std::make_shared<UseCases::Dispensing::DXF::CleanupDXFFilesUseCase>(
        file_storage_port_,
        upload_base_dir_);
}

template<>
std::shared_ptr<UseCases::Dispensing::DXF::DXFDispensingExecutionUseCase>
ApplicationContainer::CreateInstance<UseCases::Dispensing::DXF::DXFDispensingExecutionUseCase>() {
    auto path_source = ResolvePort<Domain::Trajectory::Ports::IPathSourcePort>();
    if (!path_source) {
        throw std::runtime_error("IPathSourcePort 未注册");
    }

    auto interpolation_usecase = Resolve<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>();
    auto planner = std::make_shared<UseCases::Dispensing::DXF::DXFDispensingPlanner>(
        path_source,
        interpolation_usecase,
        velocity_profile_service_);
    return std::make_shared<UseCases::Dispensing::DXF::DXFDispensingExecutionUseCase>(
        planner,
        valve_port_,
        interpolation_port_,
        motion_state_port_,
        hardware_connection_port_,
        config_port_,
        event_port_,
        task_scheduler_port_);
}

}  // namespace Siligen::Application::Container
