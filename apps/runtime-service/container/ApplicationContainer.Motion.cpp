#include "ApplicationContainer.h"

#include "runtime_execution/application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/EnsureAxesReadyZeroUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/initialization/MotionInitializationUseCase.h"
#include "motion_planning/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"
#include "runtime_execution/application/usecases/motion/manual/ManualMotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime/motion/MotionControlOperationsBridge.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "runtime_execution/application/usecases/motion/trajectory/ExecuteTrajectoryUseCase.h"
#include "domain/motion/domain-services/JogController.h"
#include "domain/motion/domain-services/ReadyZeroDecisionService.h"
#include "domain/motion/domain-services/VelocityProfileService.h"
#include "services/motion/HardLimitMonitorService.h"
#include "shared/interfaces/ILoggingService.h"

#include <memory>
#include <stdexcept>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "ApplicationContainer.Motion"

namespace Siligen::Application::Container {
void ApplicationContainer::ValidateMotionPorts() {
    if (!motion_runtime_port_) {
        throw std::runtime_error("IMotionRuntimePort 未注册");
    }
    if (!interpolation_port_) {
        throw std::runtime_error("IInterpolationPort 未注册");
    }
    if (!velocity_profile_port_) {
        SILIGEN_LOG_WARNING("IVelocityProfilePort 未注册");
    }
}

void ApplicationContainer::ConfigureMotionServices() {
    auto motion_jog_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IJogControlPort>(motion_runtime_port_)
        : motion_jog_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    jog_controller_ = std::make_shared<Domain::Motion::DomainServices::JogController>(
        motion_jog_port,
        motion_state_port);
    RegisterService<Domain::Motion::DomainServices::JogController>(jog_controller_);
    SILIGEN_LOG_INFO("JogController registered");

    if (velocity_profile_port_) {
        velocity_profile_service_ =
            std::make_shared<Domain::Motion::DomainServices::VelocityProfileService>(velocity_profile_port_);
        RegisterService<Domain::Motion::DomainServices::VelocityProfileService>(velocity_profile_service_);
        SILIGEN_LOG_INFO("VelocityProfileService registered");
    } else {
        SILIGEN_LOG_WARNING("VelocityProfileService not registered: IVelocityProfilePort missing");
    }

    if (io_control_port && position_control_port) {
        hard_limit_monitor_ =
            std::make_shared<Siligen::Application::Services::Motion::HardLimitMonitorService>(
                io_control_port,
                position_control_port);
        SILIGEN_LOG_INFO("HardLimitMonitorService registered");
    } else {
        SILIGEN_LOG_WARNING("HardLimitMonitorService not registered: IO or position control port missing");
    }
}

template<>
std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::HomeAxesUseCase>() {
    auto motion_connection_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(motion_runtime_port_)
        : motion_connection_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    return std::make_shared<UseCases::Motion::Homing::HomeAxesUseCase>(
        homing_port,
        config_port_,
        motion_connection_port,
        event_port_,
        motion_state_port);
}

template<>
std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>() {
    return std::make_shared<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>(
        Resolve<UseCases::Motion::Homing::HomeAxesUseCase>(),
        Resolve<UseCases::Motion::Manual::ManualMotionControlUseCase>(),
        Resolve<UseCases::Motion::Monitoring::MotionMonitoringUseCase>(),
        config_port_,
        std::make_shared<Domain::Motion::DomainServices::ReadyZeroDecisionService>());
}

template<>
std::shared_ptr<UseCases::Motion::MotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::MotionControlUseCase>() {
    auto home_use_case = Resolve<UseCases::Motion::Homing::HomeAxesUseCase>();
    auto ensure_ready_zero_use_case = Resolve<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>();
    auto manual_use_case = Resolve<UseCases::Motion::Manual::ManualMotionControlUseCase>();
    auto monitoring_use_case = Resolve<UseCases::Motion::Monitoring::MotionMonitoringUseCase>();
    auto operations = Siligen::Runtime::Service::Motion::CreateMotionOperations(
        std::move(home_use_case),
        std::move(ensure_ready_zero_use_case),
        std::move(manual_use_case),
        std::move(monitoring_use_case));

    return std::make_shared<UseCases::Motion::MotionControlUseCase>(
        std::move(operations.homing_operations),
        std::move(operations.manual_operations),
        std::move(operations.monitoring_operations));
}

template<>
std::shared_ptr<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    return std::make_shared<UseCases::Motion::Trajectory::ExecuteTrajectoryUseCase>(
        position_control_port,
        trigger_port_,
        event_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Manual::ManualMotionControlUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    return std::make_shared<UseCases::Motion::Manual::ManualMotionControlUseCase>(
        position_control_port,
        jog_controller_,
        homing_port);
}

template<>
std::shared_ptr<UseCases::Motion::Initialization::MotionInitializationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Initialization::MotionInitializationUseCase>() {
    auto motion_connection_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionConnectionPort>(motion_runtime_port_)
        : motion_connection_port_;
    auto axis_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(motion_runtime_port_)
        : axis_control_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    return std::make_shared<UseCases::Motion::Initialization::MotionInitializationUseCase>(
        motion_connection_port,
        axis_control_port,
        io_control_port);
}

template<>
std::shared_ptr<UseCases::Motion::Safety::MotionSafetyUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Safety::MotionSafetyUseCase>() {
    auto position_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IPositionControlPort>(motion_runtime_port_)
        : position_control_port_;
    return std::make_shared<UseCases::Motion::Safety::MotionSafetyUseCase>(position_control_port);
}

template<>
std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Monitoring::MotionMonitoringUseCase>() {
    auto motion_state_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IMotionStatePort>(motion_runtime_port_)
        : motion_state_port_;
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto homing_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IHomingPort>(motion_runtime_port_)
        : homing_port_;
    return std::make_shared<UseCases::Motion::Monitoring::MotionMonitoringUseCase>(
        motion_state_port,
        io_control_port,
        homing_port,
        interpolation_port_,
        diagnostics_port_,
        event_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Coordination::MotionCoordinationUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Coordination::MotionCoordinationUseCase>() {
    auto io_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort>(motion_runtime_port_)
        : io_control_port_;
    auto axis_control_port = motion_runtime_port_
        ? std::static_pointer_cast<Domain::Motion::Ports::IAxisControlPort>(motion_runtime_port_)
        : axis_control_port_;
    return std::make_shared<UseCases::Motion::Coordination::MotionCoordinationUseCase>(
        interpolation_port_,
        io_control_port,
        axis_control_port,
        trigger_port_);
}

template<>
std::shared_ptr<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>
ApplicationContainer::CreateInstance<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>() {
    return std::make_shared<UseCases::Motion::Interpolation::InterpolationPlanningUseCase>();
}

}  // namespace Siligen::Application::Container

