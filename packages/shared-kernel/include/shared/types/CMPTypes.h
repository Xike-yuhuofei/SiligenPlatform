// CMPTypes.h - 统一的CMP触发相关类型定义 (Unified CMP trigger related types)
// Task: T016 - Phase 2 基础设施 - 共享类型系统 (解决命名冲突)
// Task: T033 - Phase 3 - 合并core/CMPTypes.h和hardware/cmp/CMPTypes.h到此文件
// 说明: 这是统一的CMPTypes定义，解决了core/CMPTypes.h和其他位置的冲突
#pragma once

#include "Error.h"
#include "Point2D.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Types {

// CMP触发模式 (CMP trigger mode)
enum class CMPTriggerMode : int32 {
    SINGLE_POINT = 0,   // 单点触发 (Single point trigger)
    RANGE = 1,          // 范围触发 (Range trigger)
    SEQUENCE = 2,       // 序列触发 (Sequence trigger)
    REPEAT = 3,         // 重复触发 (Repeat trigger)
    POSITION_SYNC = 4,  // 位置同步触发 (Position sync trigger)
    TIME_SYNC = 5,      // 时间同步触发 (Time sync trigger)
    HYBRID = 6,         // 混合触发 (Hybrid trigger)
    PATTERN_BASED = 7   // 基于模式触发 (Pattern-based trigger)
};

// CMP触发模式转字符串 (CMP trigger mode to string)
inline const char* CMPTriggerModeToString(CMPTriggerMode mode) {
    switch (mode) {
        case CMPTriggerMode::SINGLE_POINT:
            return "SINGLE_POINT";
        case CMPTriggerMode::RANGE:
            return "RANGE";
        case CMPTriggerMode::SEQUENCE:
            return "SEQUENCE";
        case CMPTriggerMode::REPEAT:
            return "REPEAT";
        case CMPTriggerMode::POSITION_SYNC:
            return "POSITION_SYNC";
        case CMPTriggerMode::TIME_SYNC:
            return "TIME_SYNC";
        case CMPTriggerMode::HYBRID:
            return "HYBRID";
        case CMPTriggerMode::PATTERN_BASED:
            return "PATTERN_BASED";
        default:
            return "UNKNOWN";
    }
}

// CMP信号类型 (CMP signal type) - 来自core/Types.h
enum class CMPSignalType : int32 {
    PULSE = 0,  // 脉冲信号 (Pulse signal)
    LEVEL = 1   // 电平信号 (Level signal)
};

// CMP信号类型转字符串 (CMP signal type to string)
inline const char* CMPSignalTypeToString(CMPSignalType type) {
    switch (type) {
        case CMPSignalType::PULSE:
            return "PULSE";
        case CMPSignalType::LEVEL:
            return "LEVEL";
        default:
            return "UNKNOWN";
    }
}

// 点胶动作类型 (Dispensing action type)
enum class DispensingAction : int32 {
    NONE = 0,        // 无动作 (No action)
    PULSE = 1,       // 脉冲 (Pulse)
    CONTINUOUS = 2,  // 连续 (Continuous)
    DELAYED = 3      // 延时 (Delayed)
};

// 点胶动作转字符串 (Dispensing action to string)
inline const char* DispensingActionToString(DispensingAction action) {
    switch (action) {
        case DispensingAction::NONE:
            return "NONE";
        case DispensingAction::PULSE:
            return "PULSE";
        case DispensingAction::CONTINUOUS:
            return "CONTINUOUS";
        case DispensingAction::DELAYED:
            return "DELAYED";
        default:
            return "UNKNOWN";
    }
}

// CMP触发点 (CMP trigger point)
struct CMPTriggerPoint {
    int32 position;           // 触发位置 (脉冲) (Trigger position in pulses)
    DispensingAction action;  // 触发动作 (Trigger action)
    int32 pulse_width_us;     // 脉冲宽度 (微秒) (Pulse width in microseconds)
    int32 delay_time_us;      // 延时时间 (微秒) (Delay time in microseconds)
    bool is_enabled;          // 是否启用 (Is enabled)

    // 默认构造函数 (Default constructor)
    CMPTriggerPoint()
        : position(0),
          action(DispensingAction::PULSE),
          pulse_width_us(2000)  // 默认2ms
          ,
          delay_time_us(0),
          is_enabled(true) {}

    // 带参数构造函数 (Parameterized constructor)
    CMPTriggerPoint(int32 pos, DispensingAction act, int32 pulse_width = 2000, int32 delay = 0)
        : position(pos), action(act), pulse_width_us(pulse_width), delay_time_us(delay), is_enabled(true) {}

    // 验证触发点参数是否有效 (Validate trigger point parameters)
    bool Validate() const {
        // 位置范围: 0 到 1000000 脉冲
        if (position < 0 || position > 1000000) {
            return false;
        }

        // 脉冲宽度范围: 1μs 到 100000μs (100ms)
        if (pulse_width_us < 1 || pulse_width_us > 100000) {
            return false;
        }

        // 延时时间范围: 0μs 到 1000000μs (1s)
        if (delay_time_us < 0 || delay_time_us > 1000000) {
            return false;
        }

        return true;
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[CMPTriggerPoint] ";
        result += "位置: " + std::to_string(position) + " 脉冲, ";
        result += "动作: " + std::string(DispensingActionToString(action)) + ", ";
        result += "脉冲宽度: " + std::to_string(pulse_width_us) + "μs, ";
        result += "延时: " + std::to_string(delay_time_us) + "μs, ";
        result += "启用: " + std::string(is_enabled ? "是" : "否");
        return result;
    }
};

// CMP配置 (CMP configuration)
struct CMPConfiguration {
    CMPTriggerMode trigger_mode;                  // 触发模式 (Trigger mode)
    std::vector<CMPTriggerPoint> trigger_points;  // 触发点列表 (Trigger point list)
    int32 start_position;                         // 起始位置 (脉冲) (Start position in pulses)
    int32 end_position;                           // 结束位置 (脉冲) (End position in pulses)
    bool is_enabled;                              // 是否启用 (Is enabled)

    // 补偿相关字段 (Compensation related fields) - 用于CMPCompensation.cpp
    bool enable_compensation = false;           // 是否启用补偿 (Enable compensation)
    float32 compensation_factor = 1.0f;         // 补偿系数 (Compensation factor)
    float32 trigger_position_tolerance = 0.1f;  // 触发位置容差 (Trigger position tolerance)
    float32 time_tolerance_ms = 1.0f;           // 时间容差 (毫秒) (Time tolerance in ms)

    // 多通道相关字段 (Multi-channel related fields) - 用于CMPValidator.cpp
    bool enable_multi_channel = false;   // 是否启用多通道 (Enable multi-channel)
    std::vector<int32> channel_mapping;  // 通道映射 (Channel mapping)

    // CMPCoordinatedInterpolator.cpp 需要的字段
    int32 cmp_channel = 0;        // CMP通道
    int32 pulse_width_us = 2000;  // 脉冲宽度 (微秒)

    // 默认构造函数 (Default constructor)
    CMPConfiguration()
        : trigger_mode(CMPTriggerMode::SINGLE_POINT),
          trigger_points(),
          start_position(0),
          end_position(0),
          is_enabled(true),
          enable_compensation(false),
          compensation_factor(1.0f),
          trigger_position_tolerance(0.1f),
          time_tolerance_ms(1.0f),
          enable_multi_channel(false),
          channel_mapping(),
          cmp_channel(1)  // 修复: 默认通道1 (有效范围1-4)
          ,
          pulse_width_us(2000) {}

    // 添加触发点 (Add trigger point)
    void AddTriggerPoint(const CMPTriggerPoint& point) {
        trigger_points.push_back(point);
    }

    // 清除所有触发点 (Clear all trigger points)
    void ClearTriggerPoints() {
        trigger_points.clear();
    }

    // 获取触发点数量 (Get trigger point count)
    size_t GetTriggerPointCount() const {
        return trigger_points.size();
    }

    // 验证配置是否有效 (Validate configuration)
    bool Validate(std::string* error_msg = nullptr) const {
        // 检查CMP通道号 (CMPValidator.cpp要求1-4范围)
        if (cmp_channel < 1 || cmp_channel > 4) {
            if (error_msg) *error_msg = "CMP通道号必须在1-4范围内 (当前值: " + std::to_string(cmp_channel) + ")";
            return false;
        }

        // 检查脉冲宽度 (CMPValidator.cpp要求1-100000μs范围)
        if (pulse_width_us < 1 || pulse_width_us > 100000) {
            if (error_msg)
                *error_msg = "脉冲宽度必须在1-100000μs范围内 (当前值: " + std::to_string(pulse_width_us) + "μs)";
            return false;
        }

        // 检查触发点列表是否为空
        if (trigger_points.empty() && trigger_mode != CMPTriggerMode::RANGE) {
            if (error_msg)
                *error_msg =
                    "非RANGE模式触发点不能为空 (当前模式: " + std::string(CMPTriggerModeToString(trigger_mode)) + ")";
            return false;
        }

        // 检查起始和结束位置
        if (start_position < 0 || end_position < 0) {
            if (error_msg)
                *error_msg = "起始/结束位置不能为负数 (start: " + std::to_string(start_position) +
                             ", end: " + std::to_string(end_position) + ")";
            return false;
        }

        if (trigger_mode == CMPTriggerMode::RANGE && start_position >= end_position) {
            if (error_msg)
                *error_msg = "RANGE模式下起始位置必须小于结束位置 (start: " + std::to_string(start_position) +
                             " >= end: " + std::to_string(end_position) + ")";
            return false;
        }

        // 检查触发位置容差 (CMPValidator.cpp要求 > 0)
        if (trigger_position_tolerance <= 0.0f) {
            if (error_msg)
                *error_msg = "触发位置容差必须大于0 (当前值: " + std::to_string(trigger_position_tolerance) + ")";
            return false;
        }

        // 检查时间容差 (CMPValidator.cpp要求 > 0)
        if (time_tolerance_ms <= 0.0f) {
            if (error_msg) *error_msg = "时间容差必须大于0 (当前值: " + std::to_string(time_tolerance_ms) + "ms)";
            return false;
        }

        // 检查多通道配置
        if (enable_multi_channel && channel_mapping.empty()) {
            if (error_msg) *error_msg = "启用多通道时必须提供通道映射";
            return false;
        }

        // 检查所有触发点的有效性
        for (size_t i = 0; i < trigger_points.size(); ++i) {
            if (!trigger_points[i].Validate()) {
                if (error_msg)
                    *error_msg = "触发点[" + std::to_string(i) + "]验证失败: " + trigger_points[i].ToString();
                return false;
            }
        }

        return true;
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[CMPConfiguration]\n";
        result += "触发模式: " + std::string(CMPTriggerModeToString(trigger_mode)) + "\n";
        result += "起始位置: " + std::to_string(start_position) + " 脉冲\n";
        result += "结束位置: " + std::to_string(end_position) + " 脉冲\n";
        result += "触发点数量: " + std::to_string(trigger_points.size()) + "\n";
        result += "启用: " + std::string(is_enabled ? "是" : "否") + "\n";

        if (!trigger_points.empty()) {
            result += "触发点列表:\n";
            for (size_t i = 0; i < trigger_points.size(); ++i) {
                result += "  [" + std::to_string(i) + "] " + trigger_points[i].ToString() + "\n";
            }
        }

        return result;
    }
};

// CMP状态 (CMP status)
struct CMPStatus {
    bool is_active;               // 是否激活 (Is active)
    int32 current_position;       // 当前位置 (脉冲) (Current position in pulses)
    int32 next_trigger_position;  // 下一个触发位置 (Next trigger position)
    int32 trigger_count;          // 已触发次数 (Trigger count)
    bool has_error;               // 是否有错误 (Has error)
    ErrorCode last_error_code;    // 最后错误代码 (Last error code)

    // 默认构造函数 (Default constructor)
    CMPStatus()
        : is_active(false),
          current_position(0),
          next_trigger_position(0),
          trigger_count(0),
          has_error(false),
          last_error_code(ErrorCode::SUCCESS) {}

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[CMPStatus] ";
        result += "激活: " + std::string(is_active ? "是" : "否") + ", ";
        result += "当前位置: " + std::to_string(current_position) + " 脉冲, ";
        result += "下一个触发位置: " + std::to_string(next_trigger_position) + " 脉冲, ";
        result += "已触发次数: " + std::to_string(trigger_count) + ", ";
        result += "错误: " + std::string(has_error ? "是" : "否");

        if (has_error) {
            result += " (错误码: " + std::to_string(static_cast<int32>(last_error_code)) + ")";
        }

        return result;
    }
};

// ============================================================
// 硬件相关CMP类型 (来自hardware/cmp/CMPTypes.h)
// Hardware-related CMP types (from hardware/cmp/CMPTypes.h)
// ============================================================

// CMP缓冲区数据 (CMP buffer data)
struct CMPBufferData {
    std::vector<int32> position_buffer;  // 位置缓冲区 (Position buffer)
    std::vector<int16> time_buffer;      // 时间缓冲区 (Time buffer)
    bool abs_position_flag;              // 绝对位置标志 (Absolute position flag)
    int32 compare_source_axis;           // 位置比较源轴号 (MC_CmpBufData nCmpEncodeNum, SDK 1-based axis)
    CMPTriggerMode trigger_mode;         // 触发模式 (Trigger mode)

    // 默认构造函数 (Default constructor)
    CMPBufferData() : abs_position_flag(false), compare_source_axis(1), trigger_mode(CMPTriggerMode::SINGLE_POINT) {}

    // 清除缓冲区 (Clear buffer)
    void Clear() {
        position_buffer.clear();
        time_buffer.clear();
    }

    // 验证缓冲区数据 (Validate buffer data)
    bool IsValid() const {
        if (position_buffer.empty()) return false;
        if (!time_buffer.empty() && time_buffer.size() != position_buffer.size()) return false;
        return true;
    }
};

// CMP输出状态 (CMP output status)
struct CMPOutputStatus {
    int16 channel;              // CMP通道 (CMP channel)
    bool is_active;             // 是否激活 (Is active)
    bool is_configured;         // 是否已配置 (Is configured)
    CMPSignalType signal_type;  // 信号类型 (Signal type)
    int32 trigger_count;        // 触发计数 (Trigger count)
    int32 buffer_count;         // 缓冲区计数 (Buffer count)
    int64 last_trigger_time;    // 最后触发时间 (Last trigger time)
    int64 last_config_time;     // 最后配置时间 (Last config time)
    std::string last_error;     // 最后错误信息 (Last error message)

    // 默认构造函数 (Default constructor)
    CMPOutputStatus()
        : channel(1),
          is_active(false),
          is_configured(false),
          signal_type(CMPSignalType::PULSE),
          trigger_count(0),
          buffer_count(0),
          last_trigger_time(0),
          last_config_time(0) {}
};

// CMP脉冲配置 (CMP pulse configuration)
struct CMPPulseConfiguration {
    int16 channel;              // CMP通道 (CMP channel)
    CMPSignalType signal_type;  // 信号类型 (Signal type)
    int16 pulse_type1;          // 第一阶段脉冲类型 (First stage pulse type: 0=low, 1=high)
    int16 pulse_type2;          // 第二阶段脉冲类型 (Second stage pulse type)
    int32 time1_us;             // 第一阶段时间(us) (First stage time in microseconds)
    int32 time2_us;             // 第二阶段时间(us) (Second stage time in microseconds)
    int16 time_flag1;           // 第一阶段时间标志 (First stage time flag)
    int16 time_flag2;           // 第二阶段时间标志 (Second stage time flag)

    // 默认构造函数 (Default constructor)
    CMPPulseConfiguration()
        : channel(1),
          signal_type(CMPSignalType::PULSE),
          pulse_type1(1),
          pulse_type2(0),
          time1_us(1000),
          time2_us(1000),
          time_flag1(1),
          time_flag2(0) {}

    // 验证配置 (Validate configuration)
    bool IsValid() const {
        if (channel < 1 || channel > 2) return false;
        if (time1_us < 0 || time1_us > 100000) return false;
        if (time2_us < 0 || time2_us > 100000) return false;
        if (pulse_type1 < 0 || pulse_type1 > 2) return false;
        if (pulse_type2 < 0 || pulse_type2 > 2) return false;
        return true;
    }

    // 重置为默认值 (Reset to defaults)
    void ResetToDefaults() {
        channel = 1;
        signal_type = CMPSignalType::PULSE;
        pulse_type1 = 1;
        pulse_type2 = 0;
        time1_us = 1000;
        time2_us = 1000;
        time_flag1 = 1;
        time_flag2 = 0;
    }
};

// ============================================================
// 插补相关CMP类型 (来自core/CMPTypes.h)
// Interpolation-related CMP types (from core/CMPTypes.h)
// ============================================================

// 点胶触发点 (Dispensing trigger point) - 用于插补系统
struct DispensingTriggerPoint {
    Point2D position;               // 触发位置 (Trigger position)
    float32 trigger_distance;       // 触发距离 (Trigger distance)
    uint32 sequence_id;             // 序列ID (Sequence ID)
    uint32 pulse_width_us;          // 脉冲宽度(us) (Pulse width in microseconds)
    float32 pre_trigger_delay_ms;   // 触发前延时(ms) (Pre-trigger delay in milliseconds)
    float32 post_trigger_delay_ms;  // 触发后延时(ms) (Post-trigger delay in milliseconds)
    bool is_enabled;                // 是否启用 (Is enabled)

    // 默认构造函数 (Default constructor)
    DispensingTriggerPoint()
        : trigger_distance(0.0f),
          sequence_id(0),
          pulse_width_us(2000),
          pre_trigger_delay_ms(0.0f),
          post_trigger_delay_ms(0.0f),
          is_enabled(true) {}
};

// 触发时间线 (Trigger timeline) - 用于轨迹生成
struct TriggerTimeline {
    std::vector<float32> trigger_times;      // 触发时间列表 (Trigger times)
    std::vector<Point2D> trigger_positions;  // 触发位置列表 (Trigger positions)
    std::vector<uint32> pulse_widths;        // 脉冲宽度列表 (Pulse widths)
    std::vector<uint16> cmp_channels;        // CMP通道列表 (CMP channels)

    // 默认构造函数 (Default constructor)
    TriggerTimeline() = default;
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen

// ============================================================
// 向后兼容别名 (Backward compatibility aliases)
// 这些别名允许现有代码逐步迁移到新的命名空间
// ============================================================
namespace Siligen {
// 核心类型别名 (Core type aliases)
using CMPTriggerMode = Shared::Types::CMPTriggerMode;
using CMPSignalType = Shared::Types::CMPSignalType;
using DispensingAction = Shared::Types::DispensingAction;
using CMPTriggerPoint = Shared::Types::CMPTriggerPoint;
using CMPConfiguration = Shared::Types::CMPConfiguration;
using CMPStatus = Shared::Types::CMPStatus;

// 硬件相关类型别名 (Hardware-related type aliases)
using CMPBufferData = Shared::Types::CMPBufferData;
using CMPOutputStatus = Shared::Types::CMPOutputStatus;
using CMPPulseConfiguration = Shared::Types::CMPPulseConfiguration;

// 插补相关类型别名 (Interpolation-related type aliases)
using DispensingTriggerPoint = Shared::Types::DispensingTriggerPoint;
using TriggerTimeline = Shared::Types::TriggerTimeline;
}  // namespace Siligen

// 为Interpolation命名空间提供别名 (用于core/CMPTypes.h兼容)
// Aliases for Interpolation namespace (for core/CMPTypes.h compatibility)
namespace Interpolation {
using CMPTriggerMode = Siligen::Shared::Types::CMPTriggerMode;
using CMPConfiguration = Siligen::Shared::Types::CMPConfiguration;
using DispensingTriggerPoint = Siligen::Shared::Types::DispensingTriggerPoint;
using TriggerTimeline = Siligen::Shared::Types::TriggerTimeline;
}  // namespace Interpolation
