#include "SevenSegmentSCurveProfile.h"

#include "s_curve/SCurveProfileMath.h"

#include <algorithm>

namespace Siligen::Domain::Motion::DomainServices {

Result<VelocityProfile> SevenSegmentSCurveProfile::GenerateProfile(
    float32 distance,
    const VelocityProfileConfig& config) const noexcept {

    VelocityProfile profile;
    profile.type = VelocityProfileType::S_CURVE_7_SEG;

    SCurveMath::SCurveConfig sconfig;
    sconfig.start_velocity = 0.0f;
    sconfig.max_velocity = config.max_velocity;
    sconfig.end_velocity = 0.0f;
    sconfig.max_acceleration = config.max_acceleration;
    sconfig.max_jerk = config.max_jerk;

    auto samples = SCurveMath::GenerateProfile(distance, sconfig, config.time_step);
    if (samples.empty()) {
        return Result<VelocityProfile>::Success(profile);
    }

    profile.total_time = samples.back().t;
    profile.velocities.reserve(samples.size());
    profile.accelerations.reserve(samples.size());

    for (const auto& s : samples) {
        profile.velocities.push_back(s.velocity);
        profile.accelerations.push_back(s.acceleration);
    }

    return Result<VelocityProfile>::Success(profile);
}

}  // namespace Siligen::Domain::Motion::DomainServices
