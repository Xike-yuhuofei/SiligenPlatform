#pragma once

#include <map>
#include <string>
#include <vector>

namespace Siligen::RuntimeExecution::Contracts::System {

struct RawIoStatus {
    bool limit_x_pos = false;
    bool limit_x_neg = false;
    bool limit_y_pos = false;
    bool limit_y_neg = false;
    bool estop = false;
    bool estop_known = false;
    bool door = false;
    bool door_known = false;
};

struct EffectiveInterlocksStatus {
    bool estop_active = false;
    bool estop_known = false;
    bool door_open_active = false;
    bool door_open_known = false;
    bool home_boundary_x_active = false;
    bool home_boundary_y_active = false;
    std::vector<std::string> positive_escape_only_axes;
    std::map<std::string, std::string> sources;
};

struct SupervisionStatusSnapshot {
    std::string current_state = "Unknown";
    std::string requested_state = "Unknown";
    bool state_change_in_process = false;
    std::string state_reason = "unknown";
    std::string failure_code;
    std::string failure_stage;
    bool recoverable = true;
    std::string updated_at;
};

struct RuntimeSupervisionSnapshot {
    bool connected = false;
    std::string connection_state = "disconnected";
    bool interlock_latched = false;
    std::string active_job_id;
    std::string active_job_state;
    RawIoStatus io;
    EffectiveInterlocksStatus effective_interlocks;
    SupervisionStatusSnapshot supervision;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
