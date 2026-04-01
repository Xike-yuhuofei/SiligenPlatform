#pragma once

#include "runtime_execution/contracts/system/RuntimeSupervisionSnapshot.h"

#include <map>
#include <string>

namespace Siligen::RuntimeExecution::Contracts::System {

struct RuntimeAxisStatusSnapshot {
    double position = 0.0;
    double velocity = 0.0;
    bool enabled = false;
    bool homed = false;
    std::string homing_state = "unknown";
};

struct RuntimePositionSnapshot {
    double x = 0.0;
    double y = 0.0;
};

struct RuntimeDispenserStatusSnapshot {
    bool valve_open = false;
    bool supply_open = false;
};

struct RuntimeStatusSnapshot {
    RuntimeSupervisionSnapshot supervision;
    std::map<std::string, RuntimeAxisStatusSnapshot> axes;
    bool has_position = false;
    RuntimePositionSnapshot position;
    RuntimeDispenserStatusSnapshot dispenser;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
