#include "ApplicationContainer.h"

#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/system/EmergencyStopUseCase.h"
#include "application/usecases/system/InitializeSystemUseCase.h"
#include "domain/motion/domain-services/MotionControlServiceImpl.h"
#include "domain/motion/domain-services/MotionStatusServiceImpl.h"
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
    if (!hardware_connection_port_) {
        throw std::runtime_error("IHardwareConnectionPort 未注册");
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

template<>
std::shared_ptr<UseCases::System::InitializeSystemUseCase>
ApplicationContainer::CreateInstance<UseCases::System::InitializeSystemUseCase>() {
    auto home_axes_usecase = Resolve<UseCases::Motion::Homing::HomeAxesUseCase>();
    return std::make_shared<UseCases::System::InitializeSystemUseCase>(
        config_port_,
        hardware_connection_port_,
        home_axes_usecase,
        diagnostics_port_,
        event_port_,
        hard_limit_monitor_);
}

template<>
std::shared_ptr<UseCases::System::EmergencyStopUseCase>
ApplicationContainer::CreateInstance<UseCases::System::EmergencyStopUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto cmp_service = std::make_shared<Domain::Dispensing::DomainServices::CMPService>(
        trigger_port_,
        logging_service_);
    auto dispenser_model = std::make_shared<Domain::Machine::Aggregates::Legacy::DispenserModel>();
    auto motion_control_service =
        std::make_shared<Domain::Motion::DomainServices::MotionControlServiceImpl>(position_control_port);
    auto motion_status_service =
        std::make_shared<Domain::Motion::DomainServices::MotionStatusServiceImpl>(motion_state_port);

    return std::make_shared<UseCases::System::EmergencyStopUseCase>(
        motion_control_service,
        motion_status_service,
        cmp_service,
        dispenser_model,
        logging_service_);
}

}  // namespace Siligen::Application::Container
