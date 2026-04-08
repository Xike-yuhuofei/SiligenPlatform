#include "runtime/system/MachineExecutionStatePortFactory.h"

#include "runtime/system/DispenserModelMachineExecutionStateBackend.h"

namespace Siligen::RuntimeExecution::Host::System {

std::shared_ptr<Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort>
CreateMachineExecutionStatePort() {
    return std::make_shared<Siligen::Runtime::Service::System::DispenserModelMachineExecutionStateBackend>();
}

}  // namespace Siligen::RuntimeExecution::Host::System
