#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/DispensingStrategy.h"
#include "domain/dispensing/value-objects/TriggerPlan.h"
#include "domain/dispensing/value-objects/DispenseCompensationProfile.h"

#include <vector>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::uint32;
using Siligen::Shared::Types::DispensingStrategy;
using Siligen::Domain::Dispensing::ValueObjects::TriggerPlan;
using Siligen::Domain::Dispensing::ValueObjects::SafetyBoundary;
using Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile;

struct TriggerCommandPlan {
    uint32 count = 0;
    uint32 interval_ms = 0;
    float32 estimated_duration_ms = 0.0f;
};

struct TriggerSpacingPlan {
    std::vector<float32> distances_mm;
    float32 residual_mm = 0.0f;
};

struct TriggerPlanResult {
    TriggerPlan plan;
    TriggerSpacingPlan spacing;
    std::vector<TriggerCommandPlan> commands;
    uint32 interval_ms = 0;
    bool downgrade_applied = false;
};

/**
 * @brief 触发规划器
 * @details 负责等距触发规划、分段策略触发与安全边界检查
 */
class TriggerPlanner {
   public:
    TriggerPlanner() = default;
    ~TriggerPlanner() = default;

    Result<TriggerPlanResult> Plan(float32 segment_length_mm,
                                   float32 velocity_mm_s,
                                   float32 acceleration_mm_s2,
                                   float32 spatial_interval_mm,
                                   uint32 dispenser_interval_ms,
                                   float32 residual_mm,
                                   const TriggerPlan& base_plan,
                                   const DispenseCompensationProfile& compensation) const noexcept;

   private:
    TriggerSpacingPlan BuildSpacingPlan(float32 length_mm, float32 interval_mm, float32 residual_mm) const;
    std::vector<TriggerCommandPlan> BuildSegmentedCommands(float32 length_mm,
                                                           float32 velocity_mm_s,
                                                           float32 acceleration_mm_s2,
                                                           float32 spatial_interval_mm,
                                                           uint32 min_interval_ms) const;
    std::vector<TriggerCommandPlan> BuildSubSegmentedCommands(float32 length_mm,
                                                              float32 velocity_mm_s,
                                                              float32 acceleration_mm_s2,
                                                              float32 spatial_interval_mm,
                                                              uint32 min_interval_ms,
                                                              int subsegment_count) const;
    float32 ResolveSpatialInterval(float32 spatial_interval_mm,
                                   float32 velocity_mm_s,
                                   uint32 fallback_interval_ms) const;
    Result<void> EnforceSafetyBoundary(uint32& interval_ms,
                                       float32 velocity_mm_s,
                                       float32& interval_mm,
                                       const SafetyBoundary& safety,
                                       bool& downgrade_applied) const;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
