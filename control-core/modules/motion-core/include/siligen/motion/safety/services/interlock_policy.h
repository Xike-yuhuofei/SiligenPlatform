#pragma once

#include "siligen/motion/safety/interlock_types.h"
#include "siligen/motion/safety/ports/interlock_signal_port.h"
#include "siligen/shared/numeric_types.h"
#include "siligen/shared/result.h"

namespace Siligen::Motion::Safety::Services {

class InterlockPolicy {
   public:
    static Siligen::SharedKernel::Result<InterlockDecision> EvaluateSignals(
        const InterlockSignals& signals,
        const InterlockPolicyConfig& config) noexcept;

    static Siligen::SharedKernel::Result<InterlockDecision> Evaluate(
        const Ports::InterlockSignalPort& port,
        const InterlockPolicyConfig& config) noexcept;

    static Siligen::SharedKernel::VoidResult CheckAxisForJog(
        const AxisSafetyStatus& status,
        Siligen::SharedKernel::int16 direction,
        const char* error_source = "InterlockPolicy") noexcept;

    static Siligen::SharedKernel::VoidResult CheckAxisForHoming(
        const AxisSafetyStatus& status,
        const char* axis_name,
        const char* error_source = "InterlockPolicy") noexcept;

    static bool IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept;

    static bool IsSoftLimitTriggered(
        const AxisSafetyStatus& status,
        bool* positive_limit = nullptr,
        bool* negative_limit = nullptr) noexcept;
};

}  // namespace Siligen::Motion::Safety::Services
