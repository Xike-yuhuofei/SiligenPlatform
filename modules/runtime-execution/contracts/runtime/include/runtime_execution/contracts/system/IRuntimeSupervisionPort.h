#pragma once

#include "runtime_execution/contracts/system/RuntimeSupervisionSnapshot.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class IRuntimeSupervisionPort {
   public:
    virtual ~IRuntimeSupervisionPort() = default;

    virtual Siligen::Shared::Types::Result<RuntimeSupervisionSnapshot> ReadSnapshot() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
