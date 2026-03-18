#include "sim/motion/axis_model.h"

#include <algorithm>
#include <cmath>

namespace sim {

void AxisModel::reset() {
    position_ = 0.0;
    velocity_ = 0.0;
    acceleration_ = 0.0;
}

void AxisModel::configure(double max_acceleration) {
    max_acceleration_ = std::max(0.0, max_acceleration);
}

void AxisModel::update(double target_velocity, double dt) {
    if (dt <= 0.0) {
        acceleration_ = 0.0;
        return;
    }

    const double velocity_error = target_velocity - velocity_;
    if (std::abs(velocity_error) < 1e-9 || max_acceleration_ <= 0.0) {
        acceleration_ = 0.0;
        velocity_ = target_velocity;
    } else {
        const double direction = (velocity_error > 0.0) ? 1.0 : -1.0;
        acceleration_ = direction * max_acceleration_;

        const double delta = acceleration_ * dt;
        if (std::abs(delta) >= std::abs(velocity_error)) {
            velocity_ = target_velocity;
            acceleration_ = velocity_error / dt;
        } else {
            velocity_ += delta;
        }
    }

    if (velocity_ < 0.0) {
        velocity_ = 0.0;
    }

    position_ += velocity_ * dt;
}

double AxisModel::position() const {
    return position_;
}

double AxisModel::velocity() const {
    return velocity_;
}

double AxisModel::acceleration() const {
    return acceleration_;
}

}  // namespace sim
