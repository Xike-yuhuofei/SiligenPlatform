#pragma once

#include "runtime/system/DispenserModel.h"
#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"

#include <memory>

namespace Siligen::Runtime::Service::System {

class DispenserModelMachineExecutionStateBackend final
    : public Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort {
   public:
    DispenserModelMachineExecutionStateBackend();
    explicit DispenserModelMachineExecutionStateBackend(
        std::shared_ptr<DispenserModel> dispenser_model);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<DispenserModel> dispenser_model_;
};

}  // namespace Siligen::Runtime::Service::System
