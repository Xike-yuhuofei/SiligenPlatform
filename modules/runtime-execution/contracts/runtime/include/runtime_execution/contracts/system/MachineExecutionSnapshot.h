#pragma once

#include <cstdint>
#include <string>

namespace Siligen::RuntimeExecution::Contracts::System {

enum class MachineExecutionPhase : std::uint8_t {
    Unknown = 0,
    Uninitialized,
    Initializing,
    Ready,
    Running,
    Paused,
    Fault,
    EmergencyStop
};

struct MachineExecutionSnapshot {
    MachineExecutionPhase phase = MachineExecutionPhase::Unknown;
    bool emergency_stopped = false;
    bool manual_motion_allowed = false;
    bool has_pending_tasks = false;
    std::int32_t pending_task_count = 0;
    std::string recent_error_summary;
};

}  // namespace Siligen::RuntimeExecution::Contracts::System
