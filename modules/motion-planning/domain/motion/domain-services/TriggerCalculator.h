/**
 * @file TriggerCalculator.h
 * @brief 触发计算器 - 处理 trigger/CMP 的数学计算
 * @details 仅提供轨迹距离、时间线与位置映射 helper，不承载 trigger/CMP 业务 owner 语义
 *
 * @author Claude Code
 * @date 2025-11-23
 */

#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "domain/motion/value-objects/MotionTrajectory.h"

#include <memory>
#include <optional>
#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 触发计算器
 * @details 提供 trigger/CMP 相关的轨迹数学 helper；authority trigger layout 与 TriggerPlan owner 在 M8 dispense-packaging
 */
class TriggerCalculator {
   public:
    using TrajectoryPointT = Siligen::TrajectoryPoint;

    /**
     * @brief 构造函数
     */
    TriggerCalculator() = default;

    /**
     * @brief 析构函数
     */
    ~TriggerCalculator() = default;

    /**
     * @brief 计算位置触发分布
     * @param points 路径点
     * @param spacing 触发间距
     * @return 触发点列表
     */
    std::vector<DispensingTriggerPoint> CalculatePositionTriggerDistribution(const std::vector<Point2D>& points,
                                                                             float32 spacing) const;

    /**
     * @brief 计算时间触发分布
     * @param trajectory 轨迹
     * @param interval 时间间隔
     * @return 触发时间分布
     */
    std::vector<float32> CalculateTimeTriggerDistribution(const std::vector<TrajectoryPoint>& trajectory,
                                                          float32 interval) const;

    /**
     * @brief 计算时间触发分布（统一时间轨迹）
     * @param trajectory 统一时间参数化轨迹
     * @param interval 时间间隔
     * @return 触发时间分布
     */
    std::vector<float32> CalculateTimeTriggerDistribution(const ValueObjects::MotionTrajectory& trajectory,
                                                          float32 interval) const;

    /**
     * @brief 优化触发点分布
     * @param trigger_points 原始触发点
     * @param trajectory 轨迹点
     * @param min_spacing 最小间距
     * @return 优化后的触发点
     */
    std::vector<DispensingTriggerPoint> OptimizeTriggerDistribution(
        const std::vector<DispensingTriggerPoint>& trigger_points,
        const std::vector<TrajectoryPoint>& trajectory,
        float32 min_spacing) const;

    /**
     * @brief 计算触发时间线
     * @param trajectory 基础轨迹
     * @param trigger_points 触发点
     * @param cmp_config CMP配置
     * @return 触发时间线
     */
    TriggerTimeline CalculateTriggerTimeline(const std::vector<TrajectoryPoint>& trajectory,
                                             const std::vector<DispensingTriggerPoint>& trigger_points,
                                             const CMPConfiguration& cmp_config) const;

    /**
     * @brief 计算触发时间线（统一时间轨迹）
     */
    TriggerTimeline CalculateTriggerTimeline(const ValueObjects::MotionTrajectory& trajectory,
                                             const std::vector<DispensingTriggerPoint>& trigger_points,
                                             const CMPConfiguration& cmp_config) const;

    /**
     * @brief 查找最接近的轨迹点
     * @param trajectory 轨迹
     * @param target_position 目标位置
     * @return 最接近的轨迹点索引，轨迹为空时返回 std::nullopt
     */
    std::optional<int32> FindClosestTrajectoryPoint(const std::vector<TrajectoryPoint>& trajectory,
                                                    const Point2D& target_position) const;

    std::optional<int32> FindClosestTrajectoryPoint(const ValueObjects::MotionTrajectory& trajectory,
                                                    const Point2D& target_position) const;

    /**
     * @brief 计算轨迹累积距离
     * @param trajectory 轨迹
     * @return 累积距离数组
     */
    std::vector<float32> CalculateCumulativeDistance(const std::vector<TrajectoryPoint>& trajectory) const;

    std::vector<float32> CalculateCumulativeDistance(const ValueObjects::MotionTrajectory& trajectory) const;

    /**
     * @brief 插值计算触发时间
     * @param trajectory 轨迹
     * @param target_distance 目标距离
     * @param cumulative_distances 累积距离
     * @return 触发时间
     */
    float32 InterpolateTriggerTime(const std::vector<TrajectoryPoint>& trajectory,
                                   float32 target_distance,
                                   const std::vector<float32>& cumulative_distances) const;

    float32 InterpolateTriggerTime(const ValueObjects::MotionTrajectory& trajectory,
                                   float32 target_distance,
                                   const std::vector<float32>& cumulative_distances) const;

    /**
     * @brief 查找路径上的位置点
     * @param points 路径点
     * @param target_distance 目标距离
     * @return 位置点
     */
    Point2D FindPathPosition(const std::vector<Point2D>& points, float32 target_distance) const;

   private:
};

}  // namespace Siligen::Domain::Motion

