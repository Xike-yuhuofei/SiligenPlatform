#pragma once

#include "runtime_execution/contracts/system/RuntimeStatusExportSnapshot.h"
#include "shared/types/Result.h"

namespace Siligen::RuntimeExecution::Contracts::System {

class IRuntimeStatusExportPort {
   public:
    virtual ~IRuntimeStatusExportPort() = default;

    virtual Siligen::Shared::Types::Result<RuntimeStatusExportSnapshot> ReadSnapshot() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
