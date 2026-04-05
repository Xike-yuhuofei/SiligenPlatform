#pragma once

#include "application/services/dispensing/WorkflowDispensingProcessOperations.h"
#include "application/services/dispensing/WorkflowDispensingProcessOperationsProvider.h"

namespace Siligen::Runtime::Service::Dispensing {

using IWorkflowDispensingProcessOperations =
    Siligen::Application::Services::Dispensing::IWorkflowDispensingProcessOperations;

std::shared_ptr<IWorkflowDispensingProcessOperations> CreateWorkflowDispensingProcessOperations(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

}  // namespace Siligen::Runtime::Service::Dispensing
