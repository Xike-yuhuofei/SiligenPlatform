#pragma once

#include "siligen/gateway/tcp/tcp_facade_bundle.h"

#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"
#include "runtime_execution/application/usecases/system/EmergencyStopUseCase.h"
#include "runtime_execution/application/usecases/system/InitializeSystemUseCase.h"
#include "runtime_execution/application/services/motion/DefaultMotionValidationService.h"
#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"
#include "runtime_execution/application/services/motion/MotionStatusServiceImpl.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "job_ingest/contracts/dispensing/UploadContracts.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/application/usecases/motion/ptp/MoveToPositionUseCase.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/PlanningUseCase.h"
#include "facades/tcp/TcpDispensingFacade.h"
#include "facades/tcp/TcpMotionFacade.h"
#include "facades/tcp/TcpSystemFacade.h"
#include "runtime_execution/application/usecases/motion/safety/MotionSafetyUseCase.h"

#include <memory>

namespace Siligen::Gateway::Tcp {

template <typename Resolver>
std::shared_ptr<Application::UseCases::Motion::PTP::MoveToPositionUseCase> BuildMoveToPositionUseCase(Resolver& resolver) {
    auto position_control_port = resolver.template ResolvePort<Siligen::Domain::Motion::Ports::IPositionControlPort>();
    auto motion_state_port = resolver.template ResolvePort<Siligen::Domain::Motion::Ports::IMotionStatePort>();
    if (!position_control_port || !motion_state_port) {
        return nullptr;
    }

    return std::make_shared<Application::UseCases::Motion::PTP::MoveToPositionUseCase>(
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl>(
            std::move(position_control_port)),
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl>(
            std::move(motion_state_port)),
        std::make_shared<Siligen::RuntimeExecution::Application::Services::Motion::DefaultMotionValidationService>(),
        resolver.template ResolvePort<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>(),
        nullptr);
}

template <typename Resolver>
TcpFacadeBundle BuildTcpFacadeBundle(Resolver& resolver) {
    TcpFacadeBundle bundle;

    bundle.system = std::make_shared<Application::Facades::Tcp::TcpSystemFacade>(
        resolver.template Resolve<Application::UseCases::System::InitializeSystemUseCase>(),
        resolver.template Resolve<Application::UseCases::System::EmergencyStopUseCase>());

    bundle.motion = std::make_shared<Application::Facades::Tcp::TcpMotionFacade>(
        resolver.template Resolve<Application::UseCases::Motion::MotionControlUseCase>(),
        resolver.template Resolve<Application::UseCases::Motion::Safety::MotionSafetyUseCase>(),
        BuildMoveToPositionUseCase(resolver),
        resolver.template ResolvePort<Siligen::Device::Contracts::Ports::DeviceConnectionPort>());

    bundle.dispensing = std::make_shared<Application::Facades::Tcp::TcpDispensingFacade>(
        resolver.template Resolve<Application::UseCases::Dispensing::Valve::ValveCommandUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::Valve::ValveQueryUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::DispensingExecutionUseCase>(),
        resolver.template Resolve<Siligen::JobIngest::Contracts::IUploadFilePort>(),
        resolver.template Resolve<Application::UseCases::Dispensing::PlanningUseCase>(),
        resolver.template Resolve<Application::UseCases::Dispensing::DispensingWorkflowUseCase>());

    return bundle;
}

}  // namespace Siligen::Gateway::Tcp


