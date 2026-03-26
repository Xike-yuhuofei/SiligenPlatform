#pragma once

namespace Siligen::Domain::Machine::ValueObjects {

enum class MachineState {
    Idle = 0,
    Ready = 1,
    Executing = 2,
    Paused = 3,
    Stopping = 4,
    Completed = 5,
    Aborted = 6
};

inline const char* ToString(MachineState state) noexcept {
    switch (state) {
        case MachineState::Idle:
            return "idle";
        case MachineState::Ready:
            return "ready";
        case MachineState::Executing:
            return "executing";
        case MachineState::Paused:
            return "paused";
        case MachineState::Stopping:
            return "stopping";
        case MachineState::Completed:
            return "completed";
        case MachineState::Aborted:
            return "aborted";
        default:
            return "unknown";
    }
}

}  // namespace Siligen::Domain::Machine::ValueObjects
