#pragma once

#include "runtime_execution/contracts/system/RuntimeStatusSnapshot.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class IRuntimeStatusPort {
   public:
    virtual ~IRuntimeStatusPort() = default;

    virtual Siligen::Shared::Types::Result<RuntimeStatusSnapshot> ReadSnapshot() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
