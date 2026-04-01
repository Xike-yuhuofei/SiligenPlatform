#pragma once

#include "runtime_execution/contracts/system/IMachineExecutionStatePort.h"

#include <memory>

namespace Siligen::Runtime::Host::System {

class ILegacyMachineExecutionStateBackend {
   public:
    virtual ~ILegacyMachineExecutionStateBackend() = default;

    virtual Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const = 0;
    virtual Siligen::Shared::Types::Result<void> ClearPendingTasks() = 0;
    virtual Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() = 0;
    virtual Siligen::Shared::Types::Result<void> RecoverToUninitialized() = 0;
};

class LegacyMachineExecutionStateAdapter final
    : public Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort {
   public:
    LegacyMachineExecutionStateAdapter();
    explicit LegacyMachineExecutionStateAdapter(std::shared_ptr<ILegacyMachineExecutionStateBackend> backend);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot>
    ReadSnapshot() const override;
    Siligen::Shared::Types::Result<void> ClearPendingTasks() override;
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() override;
    Siligen::Shared::Types::Result<void> RecoverToUninitialized() override;

   private:
    std::shared_ptr<ILegacyMachineExecutionStateBackend> backend_;
};

}  // namespace Siligen::Runtime::Host::System
