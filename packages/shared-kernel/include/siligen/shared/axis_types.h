#pragma once

#include "siligen/shared/numeric_types.h"

namespace Siligen::SharedKernel {

enum class LogicalAxisId : int16 {
    X = 0,
    Y = 1,
    Z = 2,
    U = 3,
    INVALID = -1
};

enum class SdkAxisId : int16 {
    X = 1,
    Y = 2,
    Z = 3,
    U = 4,
    INVALID = 0
};

[[nodiscard]] constexpr SdkAxisId ToSdkAxis(LogicalAxisId axis) noexcept {
    if (axis == LogicalAxisId::INVALID) {
        return SdkAxisId::INVALID;
    }
    return static_cast<SdkAxisId>(static_cast<int16>(axis) + 1);
}

[[nodiscard]] constexpr LogicalAxisId ToLogicalAxis(SdkAxisId axis) noexcept {
    if (axis == SdkAxisId::INVALID) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(static_cast<int16>(axis) - 1);
}

[[nodiscard]] constexpr int16 ToIndex(LogicalAxisId axis) noexcept {
    return static_cast<int16>(axis);
}

[[nodiscard]] constexpr LogicalAxisId FromIndex(int16 index) noexcept {
    if (index < 0 || index > 3) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(index);
}

[[nodiscard]] constexpr bool IsValid(LogicalAxisId axis) noexcept {
    return axis != LogicalAxisId::INVALID &&
           static_cast<int16>(axis) >= 0 &&
           static_cast<int16>(axis) <= 3;
}

[[nodiscard]] constexpr bool IsValid(SdkAxisId axis) noexcept {
    return axis != SdkAxisId::INVALID &&
           static_cast<int16>(axis) >= 1 &&
           static_cast<int16>(axis) <= 4;
}

[[nodiscard]] constexpr const char* AxisName(LogicalAxisId axis) noexcept {
    switch (axis) {
        case LogicalAxisId::X:
            return "X";
        case LogicalAxisId::Y:
            return "Y";
        case LogicalAxisId::Z:
            return "Z";
        case LogicalAxisId::U:
            return "U";
        default:
            return "INVALID";
    }
}

[[nodiscard]] constexpr short ToSdkShort(SdkAxisId axis) noexcept {
    return static_cast<short>(axis);
}

[[nodiscard]] constexpr LogicalAxisId FromUserInput(int16 user_input) noexcept {
    return FromIndex(static_cast<int16>(user_input - 1));
}

[[nodiscard]] constexpr int16 ToUserDisplay(LogicalAxisId axis) noexcept {
    if (axis == LogicalAxisId::INVALID) {
        return 0;
    }
    return static_cast<int16>(axis) + 1;
}

}  // namespace Siligen::SharedKernel
