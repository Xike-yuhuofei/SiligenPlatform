#pragma once

#include "domain/dispensing/value-objects/JobExecutionMode.h"
#include "domain/dispensing/value-objects/ProcessOutputPolicy.h"
#include "domain/machine/value-objects/MachineMode.h"

namespace Siligen::Domain::Dispensing::ValueObjects {

struct GuardDecision {
    Siligen::Domain::Machine::ValueObjects::MachineMode machine_mode =
        Siligen::Domain::Machine::ValueObjects::MachineMode::Production;
    JobExecutionMode execution_mode = JobExecutionMode::Production;
    ProcessOutputPolicy output_policy = ProcessOutputPolicy::Enabled;
    bool allow_motion = true;
    bool allow_valve = true;
    bool allow_supply = true;
    bool allow_cmp = true;

    constexpr bool AllowsAnyProcessOutput() const noexcept {
        return allow_valve || allow_supply || allow_cmp;
    }

    constexpr bool AllowsDispensingOutputs() const noexcept {
        return allow_valve && allow_supply && allow_cmp;
    }
};

}  // namespace Siligen::Domain::Dispensing::ValueObjects
