/**
 * @file ArcGeometryMath.h
 * @brief 圆弧数学工具类
 * @details 提供圆弧插补的数学计算函数
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "ArcInterpolator.h"
#include "TrajectoryInterpolatorBase.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 圆弧数学工具类
 * @details 提供圆弧插补所需的数学计算功能
 */
class ArcGeometryMath {
   public:
    /**
     * @brief 构造函数
     */
    ArcGeometryMath() = default;

    /**
     * @brief 析构函数
     */
    ~ArcGeometryMath() = default;

    /**
     * @brief 计算圆弧上的点
     * @param center 圆心
     * @param radius 半径
     * @param angle 角度（弧度）
     * @return 圆弧上的点
     */
    Point2D CalculatePointOnArc(const Point2D& center, float32 radius, float32 angle) const;

    /**
     * @brief 计算圆弧长度
     * @param radius 半径
     * @param angle_span 角度跨度（弧度）
     * @return 圆弧长度
     */
    float32 CalculateArcLength(float32 radius, float32 angle_span) const;

    /**
     * @brief 角度归一化到[-π, π]
     * @param angle 原始角度
     * @return 归一化后的角度
     */
    float32 NormalizeAngle(float32 angle) const;

    /**
     * @brief 计算两点间角度差
     * @param start_angle 起始角度
     * @param end_angle 结束角度
     * @param direction 方向
     * @return 角度差
     */
    float32 CalculateAngleDifference(float32 start_angle,
                                     float32 end_angle,
                                     ArcInterpolator::ArcDirection direction) const;

    /**
     * @brief 优化圆弧速度分布
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 优化后的速度配置
     */
    std::vector<float32> OptimizeArcVelocity(const ArcInterpolator::ArcParameters& params,
                                             const InterpolationConfig& config) const;

    /**
     * @brief 计算圆弧点胶触发位置
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 触发位置列表
     */
    std::vector<float32> CalculateArcTriggerPositions(const ArcInterpolator::ArcParameters& params,
                                                      const InterpolationConfig& config) const;

   private:
};

}  // namespace Siligen::Domain::Motion


