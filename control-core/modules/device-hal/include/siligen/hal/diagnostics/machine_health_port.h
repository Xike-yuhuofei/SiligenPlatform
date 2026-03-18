#pragma once

#include "siligen/device/contracts/state/device_state.h"
#include "siligen/device/contracts/ports/device_ports.h"

namespace Siligen::Hal::Diagnostics {

using MachineHealthSnapshot = Siligen::Device::Contracts::State::MachineHealthSnapshot;
using MachineHealthPort = Siligen::Device::Contracts::Ports::MachineHealthPort;

}  // namespace Siligen::Hal::Diagnostics
