#pragma once

#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"

#include <memory>

namespace Siligen::RuntimeExecution::Host::System {

std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>
CreateMachineExecutionStatePort();

}  // namespace Siligen::RuntimeExecution::Host::System
