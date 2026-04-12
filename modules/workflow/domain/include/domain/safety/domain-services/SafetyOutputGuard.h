#pragma once

// Public compatibility surface. Canonical implementation lives in
// modules/runtime-execution/application.

#include "runtime_execution/contracts/dispensing/GuardDecision.h"
#include "shared/types/Result.h"

namespace Siligen::Domain::Safety::DomainServices {

using Siligen::Domain::Dispensing::ValueObjects::GuardDecision;
using Siligen::Domain::Dispensing::ValueObjects::JobExecutionMode;
using Siligen::Domain::Dispensing::ValueObjects::ProcessOutputPolicy;
using Siligen::Domain::Machine::ValueObjects::MachineMode;
using Siligen::Shared::Types::Result;

class SafetyOutputGuard {
   public:
    static Result<GuardDecision> Evaluate(MachineMode machine_mode,
                                          JobExecutionMode execution_mode,
                                          ProcessOutputPolicy output_policy) noexcept;
};

}  // namespace Siligen::Domain::Safety::DomainServices
