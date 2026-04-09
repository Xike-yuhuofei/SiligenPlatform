#pragma once

#include "runtime/system/MachineExecutionStateStore.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"

#include <memory>

namespace Siligen::Runtime::Service::System {

class MachineExecutionStateBackend final
    : public Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort {
   public:
    MachineExecutionStateBackend();
    explicit MachineExecutionStateBackend(std::shared_ptr<MachineExecutionStateStore> state_store);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<MachineExecutionStateStore> state_store_;
};

}  // namespace Siligen::Runtime::Service::System
