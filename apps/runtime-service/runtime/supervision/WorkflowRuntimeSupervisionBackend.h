#pragma once

#include "application/usecases/dispensing/DispensingWorkflowUseCase.h"
#include "application/usecases/system/EmergencyStopUseCase.h"
#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Runtime::Service::Supervision {

class WorkflowRuntimeSupervisionBackend final
    : public Siligen::Runtime::Host::Supervision::IRuntimeSupervisionBackend {
   public:
    WorkflowRuntimeSupervisionBackend(
        std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case,
        std::shared_ptr<Siligen::Application::UseCases::System::EmergencyStopUseCase> emergency_stop_use_case,
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
            dispensing_workflow_use_case,
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
            dispensing_execution_use_case,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port);

    Siligen::Shared::Types::Result<Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs> ReadInputs()
        const override;

   private:
    std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::System::EmergencyStopUseCase> emergency_stop_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingWorkflowUseCase>
        dispensing_workflow_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
        dispensing_execution_use_case_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port_;
};

}  // namespace Siligen::Runtime::Service::Supervision
