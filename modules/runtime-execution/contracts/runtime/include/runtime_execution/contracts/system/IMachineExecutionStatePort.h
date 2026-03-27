#pragma once

#include "runtime_execution/contracts/system/MachineExecutionSnapshot.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class IMachineExecutionStatePort {
   public:
    virtual ~IMachineExecutionStatePort() = default;

    virtual Siligen::Shared::Types::Result<MachineExecutionSnapshot> ReadSnapshot() const = 0;
    virtual Siligen::Shared::Types::Result<void> ClearPendingTasks() = 0;
    virtual Siligen::Shared::Types::Result<void> TransitionToEmergencyStop() = 0;
    virtual Siligen::Shared::Types::Result<void> RecoverToUninitialized() = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
