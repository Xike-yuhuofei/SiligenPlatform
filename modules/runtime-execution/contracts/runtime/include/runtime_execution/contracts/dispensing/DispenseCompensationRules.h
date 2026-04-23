#pragma once

#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"

#include <cmath>

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::uint32;

inline uint32 AdjustPulseWidthMs(uint32 base_ms,
                                 const DispenseCompensationProfile& profile,
                                 bool corner_boost) noexcept {
    float32 adjusted = static_cast<float32>(base_ms);
    adjusted += profile.open_comp_ms;
    adjusted += profile.close_comp_ms;

    if (corner_boost && profile.corner_pulse_scale > 0.0f) {
        adjusted *= profile.corner_pulse_scale;
    }

    if (adjusted < 1.0f) {
        adjusted = 1.0f;
    }
    if (adjusted > 60000.0f) {
        adjusted = 60000.0f;
    }

    return static_cast<uint32>(std::llround(adjusted));
}

inline DispenserValveParams ApplyTimedCompensation(const DispenserValveParams& params,
                                                   const DispenseCompensationProfile& profile,
                                                   bool corner_boost) noexcept {
    DispenserValveParams adjusted = params;
    adjusted.durationMs = AdjustPulseWidthMs(params.durationMs, profile, corner_boost);
    if (adjusted.intervalMs < adjusted.durationMs) {
        adjusted.intervalMs = adjusted.durationMs;
    }
    return adjusted;
}

inline PositionTriggeredDispenserParams ApplyPositionCompensation(
    const PositionTriggeredDispenserParams& params,
    const DispenseCompensationProfile& profile,
    bool corner_boost) noexcept {
    PositionTriggeredDispenserParams adjusted = params;
    adjusted.pulse_width_ms = AdjustPulseWidthMs(params.pulse_width_ms, profile, corner_boost);
    return adjusted;
}

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
