#pragma once

#include "domain/machine/ports/IHardwareConnectionPort.h"

namespace Siligen::RuntimeExecution::Contracts::System {

using HardwareConnectionState = Siligen::Domain::Machine::Ports::HardwareConnectionState;
using HardwareConnectionConfig = Siligen::Domain::Machine::Ports::HardwareConnectionConfig;
using HardwareConnectionInfo = Siligen::Domain::Machine::Ports::HardwareConnectionInfo;
using HeartbeatConfig = Siligen::Domain::Machine::Ports::HeartbeatConfig;
using HeartbeatStatus = Siligen::Domain::Machine::Ports::HeartbeatStatus;
using IHardwareConnectionPort = Siligen::Domain::Machine::Ports::IHardwareConnectionPort;

}  // namespace Siligen::RuntimeExecution::Contracts::System
