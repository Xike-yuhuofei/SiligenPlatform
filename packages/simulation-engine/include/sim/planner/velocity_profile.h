#pragma once

namespace sim {

struct ProfileResult {
    double length{0.0};
    double entry_velocity{0.0};
    double exit_velocity{0.0};
    double peak_velocity{0.0};
    double acceleration_time{0.0};
    double cruise_time{0.0};
    double deceleration_time{0.0};
    double total_time{0.0};
};

class VelocityProfile {
public:
    static ProfileResult compute(double length,
                                 double entry_velocity,
                                 double exit_velocity,
                                 double max_velocity,
                                 double max_acceleration);
};

}  // namespace sim
