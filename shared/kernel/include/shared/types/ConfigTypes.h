// ConfigTypes.h - 配置相关类型 (Configuration related types)
// Task: T018 - Phase 2 基础设施 - 共享类型系统
#pragma once

#include "Error.h"
#include "Types.h"

#include <string>

namespace Siligen {
namespace Shared {
namespace Types {

// 连接配置 (Connection configuration)
struct ConnectionConfig {
    std::string local_ip;  // 本地IP地址 (Local IP address)
    std::string card_ip;   // 控制卡IP地址 (Card IP address)
    int32 port;            // 端口号 (Port number)
    int32 timeout_ms;      // 超时时间 (毫秒) (Timeout in milliseconds)
    bool auto_reconnect;   // 自动重连 (Auto reconnect)

    // 默认构造函数 (Default constructor)
    ConnectionConfig()
        : local_ip(Siligen::LOCAL_IP),
          card_ip(Siligen::CONTROL_CARD_IP),
          port(0)  // 0=系统自动分配（推荐，避免端口冲突）
          ,
          timeout_ms(5000),
          auto_reconnect(true) {}

    // 带参数构造函数 (Parameterized constructor)
    ConnectionConfig(const std::string& local, const std::string& card, int32 port_num = 0)
        : local_ip(local), card_ip(card), port(port_num), timeout_ms(5000), auto_reconnect(true) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // IP地址不能为空
        if (local_ip.empty() || card_ip.empty()) {
            return false;
        }

        // 端口范围: 0-65535 (0=系统自动分配，有效)
        if (port < 0 || port > 65535) {
            return false;
        }

        // 超时时间范围: 100ms 到 60000ms (60秒)
        if (timeout_ms < 100 || timeout_ms > 60000) {
            return false;
        }

        return true;
    }

    // 获取验证错误信息 (Get validation error message)
    std::string GetValidationError() const {
        if (local_ip.empty() || card_ip.empty()) {
            return "IP地址不能为空";
        }
        if (port < 0 || port > 65535) {
            return "端口号必须在0-65535范围内";
        }
        if (timeout_ms < 100 || timeout_ms > 60000) {
            return "超时时间必须在100-60000ms之间";
        }
        return "";
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[ConnectionConfig]\n";
        result += "本地IP: " + local_ip + "\n";
        result += "控制卡IP: " + card_ip + "\n";
        result += "端口: " + std::to_string(port) + "\n";
        result += "超时: " + std::to_string(timeout_ms) + "ms\n";
        result += "自动重连: " + std::string(auto_reconnect ? "启用" : "禁用");
        return result;
    }
};

// 运动控制配置 (Motion control configuration)
struct MotionConfig {
    float32 max_velocity;        // 最大速度 (mm/s)
    float32 max_acceleration;    // 最大加速度 (mm/s²)
    float32 max_jerk;            // 最大加加速度 (mm/s³)
    float32 position_tolerance;  // 位置误差阈值 (mm)
    float32 safety_boundary;     // 安全边界 (mm)

    // 默认构造函数 (Default constructor)
    MotionConfig()
        : max_velocity(0.0f)  // 由配置提供
          ,
          max_acceleration(0.0f)  // 由配置提供
          ,
          max_jerk(0.0f)  // 由配置提供
          ,
          position_tolerance(0.05f)  // 位置误差阈值: 0.05mm
          ,
          safety_boundary(10.0f)  // 安全边界: 10mm
    {}

    // 带参数构造函数 (Parameterized constructor)
    MotionConfig(float32 vel, float32 acc, float32 jrk, float32 tol = 0.05f, float32 safety = 10.0f)
        : max_velocity(vel), max_acceleration(acc), max_jerk(jrk), position_tolerance(tol), safety_boundary(safety) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // 最大速度必须为正数
        if (max_velocity <= 0.0f) {
            return false;
        }

        // 最大加速度必须为正数
        if (max_acceleration <= 0.0f) {
            return false;
        }

        // 最大加加速度必须为正数
        if (max_jerk <= 0.0f) {
            return false;
        }

        // 位置误差阈值应该在0.001mm到1.0mm之间
        if (position_tolerance < 0.001f || position_tolerance > 1.0f) {
            return false;
        }

        // 安全边界应该在1.0mm到100.0mm之间
        if (safety_boundary < 1.0f || safety_boundary > 100.0f) {
            return false;
        }

        return true;
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[MotionConfig]\n";
        result += "最大速度: " + std::to_string(max_velocity) + " mm/s\n";
        result += "最大加速度: " + std::to_string(max_acceleration) + " mm/s²\n";
        result += "最大加加速度: " + std::to_string(max_jerk) + " mm/s³\n";
        result += "位置误差阈值: " + std::to_string(position_tolerance) + " mm\n";
        result += "安全边界: " + std::to_string(safety_boundary) + " mm";
        return result;
    }
};

// 轴配置 (Axis configuration)
struct AxisConfig {
    int32 axis_id;           // 轴ID (Axis ID)
    std::string axis_name;   // 轴名称 (Axis name)
    float32 min_position;    // 最小位置 (mm)
    float32 max_position;    // 最大位置 (mm)
    bool enable_soft_limit;  // 启用软限位 (Enable soft limit)
    bool invert_direction;   // 反转方向 (Invert direction)

    // 默认构造函数 (Default constructor)
    AxisConfig()
        : axis_id(0),
          axis_name(""),
          min_position(0.0f),
          max_position(500.0f),
          enable_soft_limit(true),
          invert_direction(false) {}

    // 带参数构造函数 (Parameterized constructor)
    AxisConfig(int32 id, const std::string& name, float32 min_pos, float32 max_pos)
        : axis_id(id),
          axis_name(name),
          min_position(min_pos),
          max_position(max_pos),
          enable_soft_limit(true),
          invert_direction(false) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // 轴ID应该在0到7之间
        if (axis_id < 0 || axis_id > 7) {
            return false;
        }

        // 轴名称不能为空
        if (axis_name.empty()) {
            return false;
        }

        // 最大位置应该大于最小位置
        if (max_position <= min_position) {
            return false;
        }

        return true;
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[AxisConfig] ";
        result += "轴ID: " + std::to_string(axis_id) + ", ";
        result += "名称: " + axis_name + ", ";
        result += "范围: [" + std::to_string(min_position) + ", " + std::to_string(max_position) + "] mm, ";
        result += "软限位: " + std::string(enable_soft_limit ? "启用" : "禁用") + ", ";
        result += "反转方向: " + std::string(invert_direction ? "是" : "否");
        return result;
    }
};

// 系统配置 (System configuration)
struct SystemConfig {
    ConnectionConfig connection;  // 连接配置 (Connection config)
    MotionConfig motion;          // 运动配置 (Motion config)
    AxisConfig x_axis;            // X轴配置 (X-axis config)
    AxisConfig y_axis;            // Y轴配置 (Y-axis config)
    AxisConfig z_axis;            // Z轴配置 (Z-axis config, optional)

    // 默认构造函数 (Default constructor)
    SystemConfig()
        : connection(),
          motion(),
          x_axis(0, "X", 0.0f, 500.0f),
          y_axis(1, "Y", 0.0f, 500.0f),
          z_axis(2, "Z", 0.0f, 100.0f) {}

    // 验证所有配置是否有效 (Validate all configurations)
    bool Validate() const {
        return connection.Validate() && motion.Validate() && x_axis.Validate() && y_axis.Validate();
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[SystemConfig]\n";
        result += connection.ToString() + "\n";
        result += motion.ToString() + "\n";
        result += x_axis.ToString() + "\n";
        result += y_axis.ToString() + "\n";
        result += z_axis.ToString();
        return result;
    }
};

// 点胶阀配置 (Dispenser valve configuration)
struct DispenserValveConfig {
    int32 cmp_channel;      // CMP通道 (CMP channel)
    int32 pulse_type;       // 脉冲类型 (Pulse type: 0=positive, 1=negative)
    int32 abs_position_flag; // 绝对位置标志 (0=相对位置, 1=绝对位置)
    int32 cmp_axis_mask;    // CMP触发允许轴掩码 (bit0=X, bit1=Y, bit2=Z, bit3=U, 0=禁用CMP)
    int32 min_count;        // 最小计数 (Minimum count)
    int32 max_count;        // 最大计数 (Maximum count)
    int32 default_count;    // 默认计数 (Default count)
    int32 min_interval_ms;  // 最小间隔时间 (毫秒) (Minimum interval)
    int32 max_interval_ms;  // 最大间隔时间 (毫秒) (Maximum interval)
    int32 min_duration_ms;  // 最小持续时间 (毫秒) (Minimum duration)
    int32 max_duration_ms;  // 最大持续时间 (毫秒) (Maximum duration)

    // 默认构造函数 (Default constructor)
    // 注意: cmp_channel 使用 1-based 索引，与 MultiCard MC_CmpBufData API 一致
    DispenserValveConfig()
        : cmp_channel(1),  // CMP通道1 (1-based，API要求)
          pulse_type(0),
          abs_position_flag(0),
          cmp_axis_mask(0x03),  // 默认允许X/Y轴CMP触发
          min_count(1),
          max_count(1000),
          default_count(100),
          min_interval_ms(10),
          max_interval_ms(60000),
          min_duration_ms(10),
          max_duration_ms(5000) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // CMP通道范围: 1-4 (1-based，与 MultiCard API 一致)
        if (cmp_channel < 1 || cmp_channel > 4) {
            return false;
        }

        // 脉冲类型: 0 或 1
        if (pulse_type < 0 || pulse_type > 1) {
            return false;
        }

        // 轴掩码范围: 0x00-0x0F (X/Y/Z/U)
        if (cmp_axis_mask < 0 || cmp_axis_mask > 0x0F) {
            return false;
        }

        // 计数范围检查
        if (min_count < 1 || max_count > 10000 || min_count > max_count) {
            return false;
        }

        // 默认计数必须在最小和最大之间
        if (default_count < min_count || default_count > max_count) {
            return false;
        }

        // 间隔时间检查
        if (min_interval_ms < 1 || max_interval_ms > 600000 || min_interval_ms > max_interval_ms) {
            return false;
        }

        // 持续时间检查
        if (min_duration_ms < 1 || max_duration_ms > 60000 || min_duration_ms > max_duration_ms) {
            return false;
        }

        return true;
    }

    // 获取验证错误信息 (Get validation error message)
    std::string GetValidationError() const {
        if (cmp_channel < 0 || cmp_channel > 3) {
            return "CMP通道必须在0-3范围内";
        }
        if (pulse_type < 0 || pulse_type > 1) {
            return "脉冲类型必须是0或1";
        }
        if (cmp_axis_mask < 0 || cmp_axis_mask > 0x0F) {
            return "CMP轴掩码必须在0-15范围内";
        }
        if (min_count < 1 || max_count > 10000) {
            return "计数范围必须在1-10000之间";
        }
        if (min_count > max_count) {
            return "最小计数不能大于最大计数";
        }
        if (default_count < min_count || default_count > max_count) {
            return "默认计数必须在最小和最大计数之间";
        }
        return "";
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[DispenserValveConfig]\n";
        result += "CMP通道: " + std::to_string(cmp_channel) + "\n";
        result += "脉冲类型: " + std::to_string(pulse_type) + "\n";
        result += "CMP轴掩码: " + std::to_string(cmp_axis_mask) + "\n";
        result += "计数范围: [" + std::to_string(min_count) + ", " + std::to_string(max_count) + "]\n";
        result += "默认计数: " + std::to_string(default_count);
        return result;
    }
};

// 供胶阀配置 (Supply valve configuration)
struct SupplyValveConfig {
    int32 do_bit_index;   // DO位索引 (DO bit index)
    int32 do_card_index;  // DO卡索引 (DO card index)
    int32 timeout_ms;     // 超时时间 (毫秒) (Timeout in milliseconds)
    bool fail_closed;     // 失败时关闭 (Fail closed on error)

    // 默认构造函数 (Default constructor)
    SupplyValveConfig() : do_bit_index(2), do_card_index(0), timeout_ms(5000), fail_closed(true) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // DO位索引范围: 0-15
        if (do_bit_index < 0 || do_bit_index > 15) {
            return false;
        }

        // DO卡索引范围: 0-7
        if (do_card_index < 0 || do_card_index > 7) {
            return false;
        }

        // 超时时间范围: 100ms 到 60000ms
        if (timeout_ms < 100 || timeout_ms > 60000) {
            return false;
        }

        return true;
    }

    // 获取验证错误信息 (Get validation error message)
    std::string GetValidationError() const {
        if (do_bit_index < 0 || do_bit_index > 15) {
            return "DO位索引必须在0-15范围内";
        }
        if (do_card_index < 0 || do_card_index > 7) {
            return "DO卡索引必须在0-7范围内";
        }
        if (timeout_ms < 100 || timeout_ms > 60000) {
            return "超时时间必须在100-60000ms之间";
        }
        return "";
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[SupplyValveConfig]\n";
        result += "DO位索引: " + std::to_string(do_bit_index) + "\n";
        result += "DO卡索引: " + std::to_string(do_card_index) + "\n";
        result += "超时时间: " + std::to_string(timeout_ms) + "ms";
        return result;
    }
};


/// @brief 阀门协调配置 (Valve coordination configuration)
///
/// 用于DXF文件驱动的电机-阀门联动响应功能
/// 提供阀门时序与安全边界参数（规划触发点与阀门单独控制共用）
struct ValveCoordinationConfig {
    int32 dispensing_interval_ms;         ///< 点胶间隔(毫秒) Dispensing interval (ms)
    int32 dispensing_duration_ms;         ///< 点胶持续时间(毫秒) Dispensing duration (ms)
    int32 valve_open_delay_ms;            ///< 阀门开启延迟(毫秒) Valve open delay (ms)
    int32 valve_response_ms;              ///< 阀门响应时间(毫秒) Valve response time (ms)
    int32 safety_timeout_ms;              ///< 安全超时(毫秒) Safety timeout (ms)
    int32 visual_margin_ms;               ///< 视觉裕量(毫秒) Visual margin (ms)
    int32 min_interval_ms;                ///< 最小触发间隔(毫秒) Minimum interval (ms)
    int32 prebuffer_points;               ///< 预缓冲点数 Prebuffer points
    int32 enabled_valves;                 ///< 启用阀门数量 Enabled valves count (1-4)
    float32 arc_segmentation_max_degree;  ///< ARC拆分最大角度(度) ARC segmentation max angle
    float32 arc_chord_tolerance_mm;       ///< 弦高容差(毫米) Chord height tolerance (mm)

    // 默认构造函数 (Default constructor)
    ValveCoordinationConfig()
        : dispensing_interval_ms(1000),
          dispensing_duration_ms(100),
          valve_open_delay_ms(50),
          valve_response_ms(5),
          safety_timeout_ms(5000),
          visual_margin_ms(10),
          min_interval_ms(10),
          prebuffer_points(50),
          enabled_valves(2),
          arc_segmentation_max_degree(30.0f),
          arc_chord_tolerance_mm(0.1f) {}

    // 验证配置是否有效 (Validate configuration)
    bool Validate() const {
        // 点胶间隔范围: 10-60000ms
        if (dispensing_interval_ms < 10 || dispensing_interval_ms > 60000) {
            return false;
        }

        // 点胶持续时间范围: 10-5000ms
        if (dispensing_duration_ms < 10 || dispensing_duration_ms > 5000) {
            return false;
        }

        // 阀门开启延迟范围: 0-1000ms
        if (valve_open_delay_ms < 0 || valve_open_delay_ms > 1000) {
            return false;
        }

        // 阀门响应时间范围: 0-1000ms
        if (valve_response_ms < 0 || valve_response_ms > 1000) {
            return false;
        }

        // 安全超时范围: 100-60000ms
        if (safety_timeout_ms < 100 || safety_timeout_ms > 60000) {
            return false;
        }

        // 视觉裕量范围: 0-1000ms
        if (visual_margin_ms < 0 || visual_margin_ms > 1000) {
            return false;
        }

        // 最小间隔范围: 1-60000ms
        if (min_interval_ms < 1 || min_interval_ms > 60000) {
            return false;
        }

        // 预缓冲点数范围: 0-10000
        if (prebuffer_points < 0 || prebuffer_points > 10000) {
            return false;
        }

        // 启用阀门数量范围: 1-4
        if (enabled_valves < 1 || enabled_valves > 4) {
            return false;
        }

        // ARC拆分最大角度范围: 1.0-90.0度
        if (arc_segmentation_max_degree < 1.0f || arc_segmentation_max_degree > 90.0f) {
            return false;
        }

        // 弦高容差范围: 0.001-1.0mm
        if (arc_chord_tolerance_mm < 0.001f || arc_chord_tolerance_mm > 1.0f) {
            return false;
        }

        return true;
    }

    // 获取验证错误信息 (Get validation error message)
    std::string GetValidationError() const {
        if (dispensing_interval_ms < 10 || dispensing_interval_ms > 60000) {
            return "点胶间隔必须在10-60000ms范围内";
        }
        if (dispensing_duration_ms < 10 || dispensing_duration_ms > 5000) {
            return "点胶持续时间必须在10-5000ms范围内";
        }
        if (valve_open_delay_ms < 0 || valve_open_delay_ms > 1000) {
            return "阀门开启延迟必须在0-1000ms范围内";
        }
        if (valve_response_ms < 0 || valve_response_ms > 1000) {
            return "阀门响应时间必须在0-1000ms范围内";
        }
        if (safety_timeout_ms < 100 || safety_timeout_ms > 60000) {
            return "安全超时必须在100-60000ms范围内";
        }
        if (visual_margin_ms < 0 || visual_margin_ms > 1000) {
            return "视觉裕量必须在0-1000ms范围内";
        }
        if (min_interval_ms < 1 || min_interval_ms > 60000) {
            return "最小间隔必须在1-60000ms范围内";
        }
        if (prebuffer_points < 0 || prebuffer_points > 10000) {
            return "预缓冲点数必须在0-10000范围内";
        }
        if (enabled_valves < 1 || enabled_valves > 4) {
            return "启用阀门数量必须在1-4范围内";
        }
        if (arc_segmentation_max_degree < 1.0f || arc_segmentation_max_degree > 90.0f) {
            return "ARC拆分最大角度必须在1.0-90.0度范围内";
        }
        if (arc_chord_tolerance_mm < 0.001f || arc_chord_tolerance_mm > 1.0f) {
            return "弦高容差必须在0.001-1.0mm范围内";
        }
        return "";
    }

    // 转换为字符串 (Convert to string)
    std::string ToString() const {
        std::string result = "[ValveCoordinationConfig]\n";
        result += "点胶间隔: " + std::to_string(dispensing_interval_ms) + " ms\n";
        result += "点胶持续时间: " + std::to_string(dispensing_duration_ms) + " ms\n";
        result += "阀门开启延迟: " + std::to_string(valve_open_delay_ms) + " ms\n";
        result += "阀门响应时间: " + std::to_string(valve_response_ms) + " ms\n";
        result += "安全超时: " + std::to_string(safety_timeout_ms) + " ms\n";
        result += "视觉裕量: " + std::to_string(visual_margin_ms) + " ms\n";
        result += "最小间隔: " + std::to_string(min_interval_ms) + " ms\n";
        result += "预缓冲点数: " + std::to_string(prebuffer_points) + "\n";
        result += "启用阀门数量: " + std::to_string(enabled_valves) + "\n";
        result += "ARC拆分最大角度: " + std::to_string(arc_segmentation_max_degree) + " 度\n";
        result += "弦高容差: " + std::to_string(arc_chord_tolerance_mm) + " mm";
        return result;
    }
};

/// @brief 速度采样配置 (Velocity trace configuration)
struct VelocityTraceConfig {
    bool enabled = true;              ///< 是否启用速度采样
    int32 interval_ms = 50;           ///< 采样间隔(ms)
    std::string output_path = "D:\\Projects\\SiligenSuite\\logs\\velocity_trace"; ///< 输出路径(目录或完整文件路径)

    bool Validate() const {
        return interval_ms >= 1 && interval_ms <= 60000;
    }

    std::string GetValidationError() const {
        if (interval_ms < 1 || interval_ms > 60000) {
            return "速度采样间隔必须在1-60000ms范围内";
        }
        return "";
    }

    std::string ToString() const {
        std::string result = "[VelocityTraceConfig]\n";
        result += "启用: " + std::string(enabled ? "true" : "false") + "\n";
        result += "间隔: " + std::to_string(interval_ms) + " ms\n";
        result += "输出路径: " + output_path;
        return result;
    }
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
