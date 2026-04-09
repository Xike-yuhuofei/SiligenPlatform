#pragma once

#include "runtime_execution/contracts/system/MachineExecutionSnapshot.h"
#include "shared/types/Result.h"

#include <cstdint>

namespace Siligen::Runtime::Service::System {

class MachineExecutionStateStore {
   public:
    MachineExecutionStateStore();

    Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot ReadSnapshot() const;
    Siligen::Shared::Types::Result<void> SetPhase(
        Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase phase);
    Siligen::Shared::Types::Result<void> SetPendingTaskCount(std::int32_t pending_task_count);
    Siligen::Shared::Types::Result<void> ClearPendingTasks();
    Siligen::Shared::Types::Result<void> TransitionToEmergencyStop();
    Siligen::Shared::Types::Result<void> RecoverToUninitialized();

   private:
    void ApplyPhase(Siligen::RuntimeExecution::Contracts::System::MachineExecutionPhase phase);

    Siligen::RuntimeExecution::Contracts::System::MachineExecutionSnapshot snapshot_{};
};

}  // namespace Siligen::Runtime::Service::System
