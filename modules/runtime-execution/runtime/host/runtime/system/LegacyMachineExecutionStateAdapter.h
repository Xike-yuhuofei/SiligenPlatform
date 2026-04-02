#pragma once

#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"

#include <memory>

namespace Siligen::Domain::Machine::Aggregates::Legacy {
class DispenserModel;
}

namespace Siligen::RuntimeExecution::Application::System {
class LegacyDispenserModel;
}

namespace Siligen::Runtime::Host::System {

class LegacyMachineExecutionStateAdapter final
    : public Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort {
   public:
    LegacyMachineExecutionStateAdapter();
    explicit LegacyMachineExecutionStateAdapter(
        std::shared_ptr<Siligen::RuntimeExecution::Application::System::LegacyDispenserModel> dispenser_model);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<Siligen::RuntimeExecution::Application::System::LegacyDispenserModel> dispenser_model_;
};

}  // namespace Siligen::Runtime::Host::System
