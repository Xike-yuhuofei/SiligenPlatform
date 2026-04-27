/**
 * @file TrajectoryPlanner.h
 * @brief 轨迹规划器 - 处理CMP协调轨迹规划
 * @details 从CMPCoordinatedInterpolator中提取的轨迹规划逻辑
 *
 * @author Claude Code
 * @date 2025-11-23
 * @update 2026-01-16 (重构阶段2: 重命名为TrajectoryPlanner)
 */

#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

#include <utility>
#include <vector>

namespace Siligen::Domain::Motion {

// 前向声明
class CMPCoordinatedInterpolator;

/**
 * @brief 轨迹规划器
 * @details 负责处理CMP协调轨迹的规划和触发点集成
 */
class TrajectoryPlanner {
   public:
    /**
     * @brief 构造函数
     */
    TrajectoryPlanner() = default;

    /**
     * @brief 析构函数
     */
    ~TrajectoryPlanner() = default;

    /**
     * @brief 生成CMP协调轨迹
     * @param base_trajectory 基础轨迹
     * @param timeline 触发时间线
     * @param cmp_config CMP配置
     * @return CMP协调轨迹
     */
    std::vector<TrajectoryPoint> GenerateCMPCoordinatedTrajectory(const std::vector<TrajectoryPoint>& base_trajectory,
                                                                  const TriggerTimeline& timeline,
                                                                  const CMPConfiguration& cmp_config) const;

    /**
     * @brief 混合触发时间线生成
     * @param position_timeline 位置触发时间线
     * @param time_timeline 时间触发时间线
     * @return 混合时间线
     */
    TriggerTimeline GenerateHybridTimeline(const TriggerTimeline& position_timeline,
                                           const TriggerTimeline& time_timeline) const;

    /**
     * @brief 多通道CMP协调
     * @param timeline 单通道时间线
     * @param channel_mapping 通道映射
     * @return 多通道时间线
     */
    TriggerTimeline CoordinateMultiChannelCMP(const TriggerTimeline& timeline,
                                              const std::vector<uint16>& channel_mapping) const;

    /**
     * @brief 触发精度分析
     * @param trajectory 轨迹
     * @param timeline 触发时间线
     * @param cmp_config CMP配置
     * @return 精度分析结果
     */
    std::vector<float32> AnalyzeTriggerAccuracy(const std::vector<TrajectoryPoint>& trajectory,
                                                const TriggerTimeline& timeline,
                                                const CMPConfiguration& cmp_config) const;

    /**
     * @brief 计算位置触发预测
     * @param trajectory 轨迹
     * @param trigger_position 触发位置
     * @param prediction_time 预测时间
     * @return 预测的触发时间和索引
     */
    std::pair<int32, float32> PredictTriggerPosition(const std::vector<TrajectoryPoint>& trajectory,
                                                     float32 trigger_position,
                                                     float32 prediction_time) const;

   private:
    /**
     * @brief 检查触发时间是否重复
     * @param trigger_times 已有的触发时间列表
     * @param new_time 新的触发时间
     * @param tolerance 时间容差
     * @return 是否重复
     */
    bool IsDuplicateTrigger(const std::vector<float32>& trigger_times, float32 new_time, float32 tolerance) const;

    /**
     * @brief 计算触发时间差
     * @param actual_time 实际时间
     * @param target_time 目标时间
     * @return 时间差 (毫秒)
     */
    float32 CalculateTimeError(float32 actual_time, float32 target_time) const;
};

}  // namespace Siligen::Domain::Motion

