#pragma once

#include "runtime_execution/contracts/system/RuntimeSupervisionSnapshot.h"

#include <map>
#include <string>

namespace Siligen::RuntimeExecution::Contracts::System {

struct RuntimeAxisStatusExportSnapshot {
    double position = 0.0;
    double velocity = 0.0;
    bool enabled = false;
    bool homed = false;
    std::string homing_state = "unknown";
};

struct RuntimePositionExportSnapshot {
    double x = 0.0;
    double y = 0.0;
};

struct RuntimeDispenserStatusExportSnapshot {
    bool valve_open = false;
    bool supply_open = false;
};

struct RuntimeStatusExportSnapshot {
    bool connected = false;
    std::string connection_state = "disconnected";
    std::string machine_state = "Unknown";
    std::string machine_state_reason = "unknown";
    bool interlock_latched = false;
    std::string active_job_id;
    std::string active_job_state;
    RawIoStatus io;
    EffectiveInterlocksStatus effective_interlocks;
    SupervisionStatusSnapshot supervision;
    std::map<std::string, RuntimeAxisStatusExportSnapshot> axes;
    bool has_position = false;
    RuntimePositionExportSnapshot position;
    RuntimeDispenserStatusExportSnapshot dispenser;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
