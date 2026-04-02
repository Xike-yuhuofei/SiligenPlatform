#pragma once

#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"
#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include <memory>

namespace Siligen::Domain::Machine::Aggregates::Legacy {
class DispenserModel;
}

namespace Siligen::Runtime::Host::System {

class WorkflowMachineExecutionStateBackend final : public ILegacyMachineExecutionStateBackend {
   public:
    WorkflowMachineExecutionStateBackend();
    explicit WorkflowMachineExecutionStateBackend(
        std::shared_ptr<Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel> dispenser_model);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel> dispenser_model_;
};

}  // namespace Siligen::Runtime::Host::System
