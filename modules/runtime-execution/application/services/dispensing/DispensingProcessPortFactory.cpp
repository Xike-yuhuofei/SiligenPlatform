#include "runtime_execution/application/services/dispensing/DispensingProcessPortFactory.h"

#include "services/dispensing/DispensingProcessService.h"

#include <utility>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

std::shared_ptr<Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort> CreateDispensingProcessPort(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port) {
    return std::make_shared<DispensingProcessService>(
        std::move(valve_port),
        std::move(interpolation_port),
        std::move(motion_state_port),
        std::move(connection_port),
        std::move(config_port));
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
