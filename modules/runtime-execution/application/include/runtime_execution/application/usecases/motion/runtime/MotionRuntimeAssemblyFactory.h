#pragma once

#include "runtime_execution/application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "runtime_execution/application/usecases/motion/homing/HomeAxesUseCase.h"
#include "runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "runtime_execution/application/usecases/motion/ptp/MoveToPositionUseCase.h"
#include "runtime_execution/application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "runtime_execution/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.h"
#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/dispensing/ITriggerControllerPort.h"
#include "runtime_execution/contracts/motion/MotionControlService.h"
#include "runtime_execution/contracts/motion/MotionStatusService.h"
#include "runtime_execution/contracts/motion/MotionValidationService.h"
#include "runtime/contracts/system/IEventPublisherPort.h"
#include "runtime_execution/application/services/motion/runtime/IMotionRuntimeServicesProvider.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Application::UseCases::Motion::Runtime {

struct MotionRuntimeAssemblyDependencies {
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort> motion_runtime_port{};
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port{};
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> configuration_port{};
    std::shared_ptr<Domain::Supervision::Ports::IEventPublisherPort> event_publisher_port{};
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_controller_port{};
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort> machine_execution_state_port{};
    std::shared_ptr<Siligen::Application::Services::Motion::Runtime::IMotionRuntimeServicesProvider> services_provider{};
};

struct MotionRuntimeAssembly {
    std::shared_ptr<Domain::Motion::DomainServices::MotionControlService> motion_control_service{};
    std::shared_ptr<Domain::Motion::DomainServices::MotionStatusService> motion_status_service{};
    std::shared_ptr<Domain::Motion::DomainServices::MotionValidationService> motion_validation_service{};
    std::unique_ptr<Homing::HomeAxesUseCase> home_use_case{};
    std::unique_ptr<PTP::MoveToPositionUseCase> move_use_case{};
    std::unique_ptr<Coordination::MotionCoordinationUseCase> coordination_use_case{};
    std::unique_ptr<Monitoring::MotionMonitoringUseCase> monitoring_use_case{};
    std::unique_ptr<Safety::MotionSafetyUseCase> safety_use_case{};
    std::unique_ptr<Trajectory::DeterministicPathExecutionUseCase> path_execution_use_case{};

    MotionRuntimeAssembly() = default;
    MotionRuntimeAssembly(MotionRuntimeAssembly&&) noexcept = default;
    MotionRuntimeAssembly& operator=(MotionRuntimeAssembly&&) noexcept = default;
    MotionRuntimeAssembly(const MotionRuntimeAssembly&) = delete;
    MotionRuntimeAssembly& operator=(const MotionRuntimeAssembly&) = delete;
};

class MotionRuntimeAssemblyFactory {
public:
    static Siligen::Shared::Types::Result<MotionRuntimeAssembly> Create(
        MotionRuntimeAssemblyDependencies dependencies);
};

}  // namespace Siligen::Application::UseCases::Motion::Runtime
