#pragma once

#include <string>

#include "sim/scheme_c/simulation_session.h"
#include "sim/simulation_engine.h"

namespace sim {

class ResultExporter {
public:
    static std::string toJson(const SimulationResult& result, bool pretty = true);
    static void writeJsonFile(const SimulationResult& result,
                              const std::string& path,
                              bool pretty = true);
    static std::string toJson(const scheme_c::SimulationSessionResult& result, bool pretty = true);
    static void writeJsonFile(const scheme_c::SimulationSessionResult& result,
                              const std::string& path,
                              bool pretty = true);
};

}  // namespace sim
