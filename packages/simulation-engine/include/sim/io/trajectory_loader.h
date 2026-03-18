#pragma once

#include <string>

#include "sim/simulation_context.h"

namespace sim {

class TrajectoryLoader {
public:
    static SimulationContext loadFromJsonFile(const std::string& path);
};

}  // namespace sim
