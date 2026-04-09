#pragma once

// Compatibility surface: prefer the runtime-execution canonical contract when visible,
// but keep a local mirror for consumers that only receive workflow public includes.
#if __has_include("runtime_execution/contracts/safety/IInterlockSignalPort.h")
#include "runtime_execution/contracts/safety/IInterlockSignalPort.h"
#else
#include "domain/safety/value-objects/InterlockTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

namespace Siligen::Domain::Safety::Ports {

using Siligen::Domain::Safety::ValueObjects::InterlockSignals;
using Siligen::Shared::Types::Result;

class IInterlockSignalPort {
   public:
    virtual ~IInterlockSignalPort() = default;

    virtual Result<InterlockSignals> ReadSignals() const noexcept = 0;

    virtual bool IsLatched() const noexcept {
        return false;
    }

    virtual Result<void> ResetLatch() noexcept {
        return Result<void>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::NOT_IMPLEMENTED,
                "interlock latch reset not supported",
                "IInterlockSignalPort"));
    }
};

}  // namespace Siligen::Domain::Safety::Ports
#endif
