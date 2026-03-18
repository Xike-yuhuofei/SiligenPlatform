#include "sim/planner/velocity_profile.h"

#include <algorithm>
#include <cmath>

namespace sim {

ProfileResult VelocityProfile::compute(double length,
                                       double entry_velocity,
                                       double exit_velocity,
                                       double max_velocity,
                                       double max_acceleration) {
    ProfileResult result{};
    result.length = std::max(0.0, length);
    result.entry_velocity = std::max(0.0, entry_velocity);
    result.exit_velocity = std::max(0.0, exit_velocity);

    if (result.length <= 0.0 || max_acceleration <= 0.0 || max_velocity <= 0.0) {
        result.peak_velocity = std::max(result.entry_velocity, result.exit_velocity);
        return result;
    }

    const double accel = max_acceleration;
    const double entry_sq = result.entry_velocity * result.entry_velocity;
    const double exit_sq = result.exit_velocity * result.exit_velocity;
    const double theoretical_peak_sq =
        std::max(0.0, ((2.0 * accel * result.length) + entry_sq + exit_sq) / 2.0);
    const double theoretical_peak = std::sqrt(theoretical_peak_sq);

    result.peak_velocity = std::min(max_velocity, theoretical_peak);

    const double accel_distance =
        std::max(0.0, (result.peak_velocity * result.peak_velocity - entry_sq) / (2.0 * accel));
    const double decel_distance =
        std::max(0.0, (result.peak_velocity * result.peak_velocity - exit_sq) / (2.0 * accel));
    const double cruise_distance = std::max(0.0, result.length - accel_distance - decel_distance);

    result.acceleration_time =
        std::max(0.0, (result.peak_velocity - result.entry_velocity) / accel);
    result.deceleration_time =
        std::max(0.0, (result.peak_velocity - result.exit_velocity) / accel);
    result.cruise_time =
        (result.peak_velocity > 0.0) ? (cruise_distance / result.peak_velocity) : 0.0;
    result.total_time =
        result.acceleration_time + result.cruise_time + result.deceleration_time;

    return result;
}

}  // namespace sim
