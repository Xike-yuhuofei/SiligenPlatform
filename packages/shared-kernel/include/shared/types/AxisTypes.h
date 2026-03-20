#pragma once

/// @file AxisTypes.h
/// @brief 类型安全的轴号定义
/// @details 解决项目中0-based和1-based轴号约定混乱问题
/// @note 内部统一使用LogicalAxisId (0-based)，仅在SDK边界转换为SdkAxisId (1-based)

#include <cstdint>

namespace Siligen::Shared::Types {

/// @brief 逻辑轴号 (0-based, 内部使用)
/// @details X=0, Y=1, Z=2, U=3
/// @note 用于数组索引、内部接口、Port定义
enum class LogicalAxisId : int16_t {
    X = 0,
    Y = 1,
    Z = 2,
    U = 3,
    INVALID = -1
};

/// @brief SDK轴号 (1-based, MultiCard SDK使用)
/// @details X=1, Y=2, Z=3, U=4
/// @note 仅在MultiCardAdapter内部使用
enum class SdkAxisId : int16_t {
    X = 1,
    Y = 2,
    Z = 3,
    U = 4,
    INVALID = 0
};

/// @brief 轴号转换: LogicalAxisId -> SdkAxisId
/// @param axis 逻辑轴号 (0-based)
/// @return SDK轴号 (1-based)
[[nodiscard]] constexpr SdkAxisId ToSdkAxis(LogicalAxisId axis) noexcept {
    if (axis == LogicalAxisId::INVALID) {
        return SdkAxisId::INVALID;
    }
    return static_cast<SdkAxisId>(static_cast<int16_t>(axis) + 1);
}

/// @brief 轴号转换: SdkAxisId -> LogicalAxisId
/// @param axis SDK轴号 (1-based)
/// @return 逻辑轴号 (0-based)
[[nodiscard]] constexpr LogicalAxisId ToLogicalAxis(SdkAxisId axis) noexcept {
    if (axis == SdkAxisId::INVALID) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(static_cast<int16_t>(axis) - 1);
}

/// @brief 获取轴号的数组索引值
/// @param axis 逻辑轴号
/// @return 数组索引 (0-based)
[[nodiscard]] constexpr int16_t ToIndex(LogicalAxisId axis) noexcept {
    return static_cast<int16_t>(axis);
}

/// @brief 从数组索引创建逻辑轴号
/// @param index 数组索引 (0-based)
/// @return 逻辑轴号
[[nodiscard]] constexpr LogicalAxisId FromIndex(int16_t index) noexcept {
    if (index < 0 || index > 3) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(index);
}

/// @brief 验证逻辑轴号是否有效
/// @param axis 逻辑轴号
/// @return true=有效, false=无效
[[nodiscard]] constexpr bool IsValid(LogicalAxisId axis) noexcept {
    return axis != LogicalAxisId::INVALID &&
           static_cast<int16_t>(axis) >= 0 &&
           static_cast<int16_t>(axis) <= 3;
}

/// @brief 验证SDK轴号是否有效
/// @param axis SDK轴号
/// @return true=有效, false=无效
[[nodiscard]] constexpr bool IsValid(SdkAxisId axis) noexcept {
    return axis != SdkAxisId::INVALID &&
           static_cast<int16_t>(axis) >= 1 &&
           static_cast<int16_t>(axis) <= 4;
}

/// @brief 轴号名称
/// @param axis 逻辑轴号
/// @return 轴名称字符串
[[nodiscard]] constexpr const char* AxisName(LogicalAxisId axis) noexcept {
    switch (axis) {
        case LogicalAxisId::X: return "X";
        case LogicalAxisId::Y: return "Y";
        case LogicalAxisId::Z: return "Z";
        case LogicalAxisId::U: return "U";
        default: return "INVALID";
    }
}

/// @brief 获取SDK轴号的原始short值
/// @param axis SDK轴号
/// @return short值 (用于SDK调用)
[[nodiscard]] constexpr short ToSdkShort(SdkAxisId axis) noexcept {
    return static_cast<short>(axis);
}

/// @brief 从用户输入创建逻辑轴号 (用户输入1-based)
/// @param user_input 用户输入的轴号 (1-based, 如1=X, 2=Y)
/// @return 逻辑轴号 (0-based)
[[nodiscard]] constexpr LogicalAxisId FromUserInput(int16_t user_input) noexcept {
    return FromIndex(static_cast<int16_t>(user_input - 1));
}

/// @brief 转换为用户显示值 (1-based)
/// @param axis 逻辑轴号
/// @return 用户显示值 (1-based, 如X=1, Y=2)
[[nodiscard]] constexpr int16_t ToUserDisplay(LogicalAxisId axis) noexcept {
    if (axis == LogicalAxisId::INVALID) {
        return 0;
    }
    return static_cast<int16_t>(axis) + 1;
}

}  // namespace Siligen::Shared::Types
