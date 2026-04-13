#pragma once

#include "runtime_execution/application/usecases/system/EmergencyStopUseCase.h"
#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"
#include "runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
#include "runtime_execution/application/usecases/motion/MotionControlUseCase.h"
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Runtime::Service::Supervision {

class IRuntimeJobTerminalSync {
   public:
    virtual ~IRuntimeJobTerminalSync() = default;

    virtual void OnTerminalJobObserved(const std::string& job_id) const = 0;
};

class RuntimeExecutionSupervisionBackend final
    : public Siligen::Runtime::Host::Supervision::IRuntimeSupervisionBackend {
   public:
    RuntimeExecutionSupervisionBackend(
        std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case,
        std::shared_ptr<Siligen::Application::UseCases::System::EmergencyStopUseCase> emergency_stop_use_case,
        std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
            dispensing_execution_use_case,
        std::shared_ptr<Siligen::Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port);

    Siligen::Shared::Types::Result<Siligen::Runtime::Host::Supervision::RuntimeSupervisionInputs> ReadInputs()
        const override;

   private:
    std::shared_ptr<Siligen::Application::UseCases::Motion::MotionControlUseCase> motion_control_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::System::EmergencyStopUseCase> emergency_stop_use_case_;
    std::shared_ptr<Siligen::Application::UseCases::Dispensing::DispensingExecutionUseCase>
        dispensing_execution_use_case_;
    std::shared_ptr<Siligen::Domain::Safety::Ports::IInterlockSignalPort> interlock_signal_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> device_connection_port_;
};

}  // namespace Siligen::Runtime::Service::Supervision
