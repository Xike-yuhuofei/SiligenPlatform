#pragma once

// Compatibility surface: the dependent safety contracts already prefer runtime-
// execution canonical headers when visible, and fall back to workflow mirrors
// for consumers that only receive workflow public includes.
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "domain/safety/value-objects/InterlockTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <cstdint>

namespace Siligen::Domain::Safety::DomainServices {

using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Shared::Types::Result;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Safety::Ports::IInterlockSignalPort;
using Siligen::Domain::Safety::ValueObjects::InterlockCause;
using Siligen::Domain::Safety::ValueObjects::InterlockDecision;
using Siligen::Domain::Safety::ValueObjects::InterlockPolicyConfig;
using Siligen::Domain::Safety::ValueObjects::InterlockPriority;
using Siligen::Domain::Safety::ValueObjects::InterlockSignals;

class InterlockPolicy {
   public:
    static Result<InterlockDecision> EvaluateSignals(const InterlockSignals& signals,
                                                     const InterlockPolicyConfig& config) noexcept;

    static Result<InterlockDecision> Evaluate(const IInterlockSignalPort& port,
                                              const InterlockPolicyConfig& config) noexcept;

    static Result<void> CheckAxisForJog(const MotionStatus& status,
                                        int16_t direction,
                                        const char* error_source = "InterlockPolicy") noexcept;

    static Result<void> CheckAxisForHoming(const MotionStatus& status,
                                           const char* axis_name,
                                           const char* error_source = "InterlockPolicy") noexcept;

    static bool IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept;

    static bool IsSoftLimitTriggered(const MotionStatus& status,
                                     bool* positive_limit = nullptr,
                                     bool* negative_limit = nullptr) noexcept;
};

}  // namespace Siligen::Domain::Safety::DomainServices
