#pragma once

#include "runtime/system/LegacyMachineExecutionStateAdapter.h"

#include <memory>

namespace Siligen::Domain::Machine::Aggregates::Legacy {
class DispenserModel;
}

namespace Siligen::Runtime::Service::System {

class DispenserModelMachineExecutionStateBackend final
    : public Siligen::Runtime::Host::System::ILegacyMachineExecutionStateBackend {
   public:
    DispenserModelMachineExecutionStateBackend();
    explicit DispenserModelMachineExecutionStateBackend(
        std::shared_ptr<Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel> dispenser_model);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<Siligen::Domain::Machine::Aggregates::Legacy::DispenserModel> dispenser_model_;
};

}  // namespace Siligen::Runtime::Service::System
