#pragma once

namespace Siligen::Domain::Dispensing::ValueObjects {

enum class ProcessOutputPolicy {
    Enabled = 0,
    Inhibited = 1
};

inline const char* ToString(ProcessOutputPolicy policy) noexcept {
    switch (policy) {
        case ProcessOutputPolicy::Enabled:
            return "enabled";
        case ProcessOutputPolicy::Inhibited:
            return "inhibited";
        default:
            return "unknown";
    }
}

}  // namespace Siligen::Domain::Dispensing::ValueObjects
