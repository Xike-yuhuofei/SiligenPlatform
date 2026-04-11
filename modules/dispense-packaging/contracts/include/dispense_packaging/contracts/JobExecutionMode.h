#pragma once

namespace Siligen::Domain::Dispensing::ValueObjects {

enum class JobExecutionMode {
    Production = 0,
    ValidationDryCycle = 1
};

inline const char* ToString(JobExecutionMode mode) noexcept {
    switch (mode) {
        case JobExecutionMode::Production:
            return "production";
        case JobExecutionMode::ValidationDryCycle:
            return "validation_dry_cycle";
        default:
            return "unknown";
    }
}

}  // namespace Siligen::Domain::Dispensing::ValueObjects
