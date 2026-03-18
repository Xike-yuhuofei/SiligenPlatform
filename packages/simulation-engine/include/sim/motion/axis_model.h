#pragma once

namespace sim {

class AxisModel {
public:
    void reset();
    void configure(double max_acceleration);
    void update(double target_velocity, double dt);

    double position() const;
    double velocity() const;
    double acceleration() const;

private:
    double position_{0.0};
    double velocity_{0.0};
    double acceleration_{0.0};
    double max_acceleration_{500.0};
};

}  // namespace sim
