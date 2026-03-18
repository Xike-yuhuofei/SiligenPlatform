#include "sim/simulation_clock.h"

namespace sim {

SimulationClock::SimulationClock(double timestep_seconds) : timestep_(timestep_seconds) {}

void SimulationClock::reset() {
    current_time_ = 0.0;
}

void SimulationClock::step() {
    current_time_ += timestep_;
}

double SimulationClock::now() const {
    return current_time_;
}

double SimulationClock::timestep() const {
    return timestep_;
}

}  // namespace sim
