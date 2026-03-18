#pragma once

#include <deque>
#include <vector>

#include "sim/simulation_context.h"
#include "sim/simulation_engine.h"

namespace sim {

class ValveModel {
public:
    void reset();
    void configure(const ValveConfig& config);
    void setCommand(bool open, double now);
    void update(double now, double dt);

    bool actualState() const;
    bool hasPendingEvents() const;
    double valveOpenTime() const;
    const std::vector<TimelineEvent>& events() const;

private:
    struct PendingChange {
        bool state{false};
        double execute_at{0.0};
    };

    ValveConfig config_{};
    bool commanded_state_{false};
    bool actual_state_{false};
    double valve_open_time_{0.0};
    std::deque<PendingChange> pending_{};
    std::vector<TimelineEvent> events_{};
};

}  // namespace sim
