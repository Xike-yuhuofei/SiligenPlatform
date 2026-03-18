// TrajectoryTypes.h - 轨迹生成相关类型 (Trajectory generation related types)
// 职责: 定义轨迹生成和插补所需的配置和结果类型
// 架构合规性: 共享层零依赖，纯数据结构
#pragma once

#include "Point.h"
#include "Types.h"

#include <string>
#include <vector>

namespace Siligen {
namespace Shared {
namespace Types {

/**
 * @brief 轨迹生成配置
 *
 * 定义轨迹插补和速度规划的参数。
 */
struct TrajectoryConfig {
    float32 max_velocity = 100.0f;       // mm/s - 最大速度
    float32 max_acceleration = 500.0f;   // mm/s^2 - 最大加速度
    float32 max_jerk = 5000.0f;          // mm/s^3 - 最大加加速度
    float32 time_step = 0.001f;          // s - 采样周期
    float32 arc_tolerance = 0.01f;       // mm - 圆弧插补精度
    float32 dispensing_interval = 0.0f;  // mm - 点胶间隔，0 表示不启用
    uint16 trigger_pulse_us = 100;       // us - 触发脉宽
};

/**
 * @brief 轨迹生成结果
 *
 * 包含生成的轨迹点和统计信息。
 */
struct TrajectoryResult {
    bool success = false;                    // 是否成功
    std::string error_message;               // 错误消息
    std::vector<TrajectoryPoint> points;     // 轨迹点序列
    float32 total_length = 0;                // mm - 总路径长度
    float32 total_time = 0;                  // s - 总运动时间
    int trigger_count = 0;                   // 触发点数量
};

}  // namespace Types
}  // namespace Shared
}  // namespace Siligen
