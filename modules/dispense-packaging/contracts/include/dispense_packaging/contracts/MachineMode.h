#pragma once

namespace Siligen::Domain::Machine::ValueObjects {

enum class MachineMode {
    Production = 0,
    Test = 1
};

inline const char* ToString(MachineMode mode) noexcept {
    switch (mode) {
        case MachineMode::Production:
            return "production";
        case MachineMode::Test:
            return "test";
        default:
            return "unknown";
    }
}

}  // namespace Siligen::Domain::Machine::ValueObjects
