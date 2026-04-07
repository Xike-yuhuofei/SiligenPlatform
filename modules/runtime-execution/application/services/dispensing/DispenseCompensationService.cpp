#include "DispenseCompensationService.h"

#include <algorithm>
#include <cmath>

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

uint32 DispenseCompensationService::AdjustPulseWidthMs(uint32 base_ms,
                                                       const DispenseCompensationProfile& profile,
                                                       bool corner_boost) const noexcept {
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

DispenserValveParams DispenseCompensationService::ApplyTimedCompensation(
    const DispenserValveParams& params,
    const DispenseCompensationProfile& profile,
    bool corner_boost) const noexcept {
    DispenserValveParams adjusted = params;
    adjusted.durationMs = AdjustPulseWidthMs(params.durationMs, profile, corner_boost);
    if (adjusted.intervalMs < adjusted.durationMs) {
        adjusted.intervalMs = adjusted.durationMs;
    }
    return adjusted;
}

PositionTriggeredDispenserParams DispenseCompensationService::ApplyPositionCompensation(
    const PositionTriggeredDispenserParams& params,
    const DispenseCompensationProfile& profile,
    bool corner_boost) const noexcept {
    PositionTriggeredDispenserParams adjusted = params;
    adjusted.pulse_width_ms = AdjustPulseWidthMs(params.pulse_width_ms, profile, corner_boost);
    return adjusted;
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
