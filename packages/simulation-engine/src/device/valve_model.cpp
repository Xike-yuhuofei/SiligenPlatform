#include "sim/device/valve_model.h"

#include <algorithm>

#include "sim/control/io_model.h"

namespace sim {

void ValveModel::reset() {
    commanded_state_ = false;
    actual_state_ = false;
    valve_open_time_ = 0.0;
    pending_.clear();
    events_.clear();
}

void ValveModel::configure(const ValveConfig& config) {
    config_ = config;
}

void ValveModel::setCommand(bool open, double now) {
    if (commanded_state_ == open) {
        return;
    }

    commanded_state_ = open;
    const double delay = open ? config_.open_delay_seconds : config_.close_delay_seconds;
    pending_.push_back(PendingChange{open, now + std::max(0.0, delay)});
}

void ValveModel::update(double now, double dt) {
    while (!pending_.empty() && pending_.front().execute_at <= now + 1e-12) {
        const auto change = pending_.front();
        pending_.pop_front();

        if (actual_state_ == change.state) {
            continue;
        }

        actual_state_ = change.state;
        events_.push_back(TimelineEvent{
            now,
            channels::kValve,
            actual_state_,
            "valve-delay"
        });
    }

    if (actual_state_) {
        valve_open_time_ += std::max(0.0, dt);
    }
}

bool ValveModel::actualState() const {
    return actual_state_;
}

bool ValveModel::hasPendingEvents() const {
    return !pending_.empty();
}

double ValveModel::valveOpenTime() const {
    return valve_open_time_;
}

const std::vector<TimelineEvent>& ValveModel::events() const {
    return events_;
}

}  // namespace sim
