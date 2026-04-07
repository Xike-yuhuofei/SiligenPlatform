#pragma once

#include "process_planning/contracts/configuration/IConfigurationPort.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

std::shared_ptr<Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort> CreateDispensingProcessPort(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
