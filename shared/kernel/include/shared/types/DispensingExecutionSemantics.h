#pragma once

#include "shared/types/Types.h"

#include <optional>
#include <string_view>

namespace Siligen::Shared::Types {

enum class DispensingExecutionGeometryKind {
    PATH = 0,
    POINT = 1
};

enum class DispensingExecutionStrategy {
    FLYING_SHOT = 0,
    STATIONARY_SHOT = 1
};

enum class PointFlyingCarrierDirectionMode {
    APPROACH_DIRECTION = 0
};

struct PointFlyingCarrierPolicy {
    PointFlyingCarrierDirectionMode direction_mode =
        PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    float32 trigger_spatial_interval_mm = 0.0f;
};

inline const char* ToString(DispensingExecutionGeometryKind kind) noexcept {
    switch (kind) {
        case DispensingExecutionGeometryKind::PATH:
            return "path";
        case DispensingExecutionGeometryKind::POINT:
            return "point";
        default:
            return "unknown";
    }
}

inline const char* ToString(DispensingExecutionStrategy strategy) noexcept {
    switch (strategy) {
        case DispensingExecutionStrategy::FLYING_SHOT:
            return "flying_shot";
        case DispensingExecutionStrategy::STATIONARY_SHOT:
            return "stationary_shot";
        default:
            return "unknown";
    }
}

inline const char* ToString(PointFlyingCarrierDirectionMode mode) noexcept {
    switch (mode) {
        case PointFlyingCarrierDirectionMode::APPROACH_DIRECTION:
            return "approach_direction";
        default:
            return "unknown";
    }
}

inline bool IsValid(const PointFlyingCarrierPolicy& policy) noexcept {
    return policy.direction_mode == PointFlyingCarrierDirectionMode::APPROACH_DIRECTION &&
           policy.trigger_spatial_interval_mm > 0.0f;
}

inline std::optional<PointFlyingCarrierDirectionMode> TryParsePointFlyingCarrierDirectionMode(
    std::string_view value) noexcept {
    if (value == "approach_direction") {
        return PointFlyingCarrierDirectionMode::APPROACH_DIRECTION;
    }
    return std::nullopt;
}

}  // namespace Siligen::Shared::Types
