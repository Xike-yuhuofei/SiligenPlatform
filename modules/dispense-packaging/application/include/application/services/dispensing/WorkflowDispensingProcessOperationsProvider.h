#pragma once

#include "application/services/dispensing/WorkflowDispensingProcessOperations.h"

namespace Siligen::Application::Services::Dispensing {

class WorkflowDispensingProcessOperationsProvider {
   public:
    std::shared_ptr<IWorkflowDispensingProcessOperations> CreateOperations(
        std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port) const;
};

}  // namespace Siligen::Application::Services::Dispensing
