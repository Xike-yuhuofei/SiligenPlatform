#pragma once

#include "runtime_execution/contracts/dispensing/DispenseCompensationProfile.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "shared/types/Types.h"

namespace Siligen::RuntimeExecution::Application::Services::Dispensing {

using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Shared::Types::uint32;

/**
 * @brief 点胶工艺补偿服务
 * @details 负责起胶/收胶补偿、拐角脉宽调整
 */
class DispenseCompensationService {
   public:
    DispenseCompensationService() = default;
    ~DispenseCompensationService() = default;

    uint32 AdjustPulseWidthMs(uint32 base_ms,
                              const DispenseCompensationProfile& profile,
                              bool corner_boost) const noexcept;

    DispenserValveParams ApplyTimedCompensation(const DispenserValveParams& params,
                                                const DispenseCompensationProfile& profile,
                                                bool corner_boost) const noexcept;

    PositionTriggeredDispenserParams ApplyPositionCompensation(
        const PositionTriggeredDispenserParams& params,
        const DispenseCompensationProfile& profile,
        bool corner_boost) const noexcept;
};

}  // namespace Siligen::RuntimeExecution::Application::Services::Dispensing
