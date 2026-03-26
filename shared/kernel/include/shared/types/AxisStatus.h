// AxisStatus.h - 轴状态相关类型 (Axis status related types)
// Task: T015 - Phase 2 基础设施 - 共享类型系统
#pragma once

#include "Error.h"

#include <chrono>
#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

// 轴状态枚举 (Axis state enumeration)
enum class AxisState : int32 {
    DISABLED = 0,       // 禁用 (Disabled)
    ENABLED = 1,        // 启用 (Enabled)
    HOMING = 2,         // 回零中 (Homing)
    MOVING = 3,         // 运动中 (Moving)
    FAULT = 4,          // 错误状态 (Error state)
    EMERGENCY_STOP = 5  // 紧急停止 (Emergency stop)
};

// 轴状态字符串转换 (Axis state to string conversion)
inline const char* AxisStateToString(AxisState state) {
    switch (state) {
        case AxisState::DISABLED:
            return "DISABLED";
        case AxisState::ENABLED:
            return "ENABLED";
        case AxisState::HOMING:
            return "HOMING";
        case AxisState::MOVING:
            return "MOVING";
        case AxisState::FAULT:
            return "ERROR";
        case AxisState::EMERGENCY_STOP:
            return "EMERGENCY_STOP";
        default:
            return "UNKNOWN";
    }
}

// 轴状态信息结构 (Axis status information structure)
struct AxisStatus {
    AxisState state;                                    // 当前状态 (Current state)
    float32 current_position;                           // 当前位置 (mm)
    float32 target_position;                            // 目标位置 (mm)
    float32 current_velocity;                           // 当前速度 (mm/s)
    bool is_homed;                                      // 是否已回零 (Is homed)
    bool has_error;                                     // 是否有错误 (Has error)
    ErrorCode last_error_code;                          // 最后错误代码 (Last error code)
    std::chrono::system_clock::time_point last_update;  // 最后更新时间 (Last update time)

    // 默认构造函数 (Default constructor)
    AxisStatus()
        : state(AxisState::DISABLED),
          current_position(0.0f),
          target_position(0.0f),
          current_velocity(0.0f),
          is_homed(false),
          has_error(false),
          last_error_code(ErrorCode::SUCCESS),
          last_update(std::chrono::system_clock::now()) {}

    // 带参数构造函数 (Parameterized constructor)
    AxisStatus(AxisState state_val, float32 current_pos, float32 target_pos, float32 velocity)
        : state(state_val),
          current_position(current_pos),
          target_position(target_pos),
          current_velocity(velocity),
          is_homed(false),
          has_error(false),
          last_error_code(ErrorCode::SUCCESS),
          last_update(std::chrono::system_clock::now()) {}

    // 判断是否正在运动 (Check if moving)
    bool IsMoving() const {
        return state == AxisState::MOVING;
    }

    // 判断是否已启用 (Check if enabled)
    bool IsEnabled() const {
        return state == AxisState::ENABLED || state == AxisState::MOVING;
    }

    // 判断是否处于错误状态 (Check if in error state)
    bool IsInError() const {
        return state == AxisState::FAULT || state == AxisState::EMERGENCY_STOP || has_error;
    }

    // 判断是否可以移动 (Check if can move)
    bool CanMove() const {
        return is_homed && IsEnabled() && !IsInError();
    }

    // 计算位置误差 (Calculate position error)
    float32 GetPositionError() const {
        return std::abs(target_position - current_position);
    }

    // 判断是否到达目标位置 (Check if reached target position)
    bool HasReachedTarget(float32 tolerance = 0.05f) const {
        return GetPositionError() < tolerance;
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[AxisStatus] ";
        result += "状态: " + std::string(AxisStateToString(state)) + ", ";
        result += "当前位置: " + std::to_string(current_position) + "mm, ";
        result += "目标位置: " + std::to_string(target_position) + "mm, ";
        result += "速度: " + std::to_string(current_velocity) + "mm/s, ";
        result += "已回零: " + std::string(is_homed ? "是" : "否") + ", ";
        result += "错误: " + std::string(has_error ? "是" : "否");

        if (has_error) {
            result += " (错误码: " + std::to_string(static_cast<int32>(last_error_code)) + ")";
        }

        return result;
    }

    // 创建错误状态 (Create error status)
    static AxisStatus CreateErrorStatus(ErrorCode error_code, const std::string& error_msg = "") {
        AxisStatus status;
        status.state = AxisState::FAULT;
        status.has_error = true;
        status.last_error_code = error_code;
        status.last_update = std::chrono::system_clock::now();
        return status;
    }

    // 创建紧急停止状态 (Create emergency stop status)
    static AxisStatus CreateEmergencyStopStatus() {
        AxisStatus status;
        status.state = AxisState::EMERGENCY_STOP;
        status.has_error = true;
        status.last_error_code = ErrorCode::EMERGENCY_STOP_ACTIVATED;
        status.current_velocity = 0.0f;
        status.last_update = std::chrono::system_clock::now();
        return status;
    }

    // 更新时间戳 (Update timestamp)
    void UpdateTimestamp() {
        last_update = std::chrono::system_clock::now();
    }
};

// 多轴状态集合 (Multi-axis status collection)
struct MultiAxisStatus {
    AxisStatus x_axis;
    AxisStatus y_axis;
    AxisStatus z_axis;  // 可选 (Optional)

    // 默认构造函数 (Default constructor)
    MultiAxisStatus() = default;

    // 判断所有轴是否都已启用 (Check if all axes are enabled)
    bool AllEnabled() const {
        return x_axis.IsEnabled() && y_axis.IsEnabled();
    }

    // 判断所有轴是否都已回零 (Check if all axes are homed)
    bool AllHomed() const {
        return x_axis.is_homed && y_axis.is_homed;
    }

    // 判断是否有任何轴处于错误状态 (Check if any axis is in error state)
    bool HasAnyError() const {
        return x_axis.IsInError() || y_axis.IsInError() || z_axis.IsInError();
    }

    // 判断所有轴是否都可以移动 (Check if all axes can move)
    bool CanMove() const {
        return x_axis.CanMove() && y_axis.CanMove();
    }

    // 判断是否有任何轴正在运动 (Check if any axis is moving)
    bool AnyMoving() const {
        return x_axis.IsMoving() || y_axis.IsMoving() || z_axis.IsMoving();
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[MultiAxisStatus]\n";
        result += "X轴: " + x_axis.ToString() + "\n";
        result += "Y轴: " + y_axis.ToString() + "\n";
        result += "Z轴: " + z_axis.ToString();
        return result;
    }

    // 更新所有时间戳 (Update all timestamps)
    void UpdateTimestamps() {
        x_axis.UpdateTimestamp();
        y_axis.UpdateTimestamp();
        z_axis.UpdateTimestamp();
    }
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
