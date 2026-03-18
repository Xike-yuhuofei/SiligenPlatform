#pragma once

namespace sim {

class SimulationClock {
public:
    explicit SimulationClock(double timestep_seconds = 0.001);

    void reset();
    void step();

    double now() const;
    double timestep() const;

private:
    double current_time_{0.0};
    double timestep_{0.001};
};

}  // namespace sim
