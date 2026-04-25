#pragma once

#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Motion::DomainServices::SCurveMath {

using Siligen::Shared::Types::float32;

/// 7-segment S-curve segment times and boundary values.
struct SCurveProfileData {
    float32 t1 = 0, t2 = 0, t3 = 0, t4 = 0, t5 = 0, t6 = 0, t7 = 0;
    float32 v_peak = 0;
    float32 a_peak_accel = 0;  // actual peak acceleration in accel phase (may be < max_acceleration in triangular case)
    float32 a_peak_decel = 0;  // actual peak acceleration magnitude in decel phase

    float32 TotalTime() const noexcept {
        return t1 + t2 + t3 + t4 + t5 + t6 + t7;
    }
};

/// Configuration for S-curve profile generation.
struct SCurveConfig {
    float32 start_velocity = 0;
    float32 max_velocity = 0;
    float32 end_velocity = 0;
    float32 max_acceleration = 0;
    float32 max_jerk = 0;
};

/// A single discretized sample point.
struct SCurveSample {
    float32 t = 0;
    float32 position = 0;   ///< Distance traveled along path.
    float32 velocity = 0;
    float32 acceleration = 0;
};

/// Compute the 7-segment S-curve segment times for arbitrary start/end velocities.
/// Distance-limited profiles (cannot reach max_velocity) use binary search for the
/// achievable peak velocity.
SCurveProfileData ComputeSegmentTimes(float32 distance, const SCurveConfig& config) noexcept;

/// Evaluate velocity at time t using pre-computed segment data.
float32 VelocityAt(float32 t,
                   const SCurveProfileData& data,
                   const SCurveConfig& config) noexcept;

/// Evaluate acceleration at time t using pre-computed segment data.
float32 AccelerationAt(float32 t,
                       const SCurveProfileData& data,
                       const SCurveConfig& config) noexcept;

/// Evaluate position (distance traveled along path) at time t.
float32 PositionAt(float32 t,
                   const SCurveProfileData& data,
                   const SCurveConfig& config) noexcept;

/// Generate a uniformly time-sampled discretized profile.
std::vector<SCurveSample> GenerateProfile(float32 distance,
                                          const SCurveConfig& config,
                                          float32 time_step) noexcept;

}  // namespace Siligen::Domain::Motion::DomainServices::SCurveMath
