/**
 * @file CircleCalculator.h
 * @brief 圆参数计算器
 * @details 提供圆弧参数计算和验证功能
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "domain/motion/domain-services/interpolation/ArcInterpolator.h"
#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"

namespace Siligen::Domain::Motion {

/**
 * @brief 圆参数计算器
 * @details 实现圆弧参数计算和验证
 */
class CircleCalculator {
   public:
    /**
     * @brief 构造函数
     */
    CircleCalculator() = default;

    /**
     * @brief 析构函数
     */
    ~CircleCalculator() = default;

    /**
     * @brief 计算圆弧参数（三点）
     * @param p1 第一个点
     * @param p2 第二个点
     * @param p3 第三个点
     * @return 圆弧参数
     */
    ArcInterpolator::ArcParameters CalculateArcParameters(const Point2D& p1,
                                                          const Point2D& p2,
                                                          const Point2D& p3) const;

    /**
     * @brief 计算圆弧参数（圆心+两点）
     * @param center 圆心
     * @param start_point 起点
     * @param end_point 终点
     * @param direction 圆弧方向
     * @return 圆弧参数
     */
    ArcInterpolator::ArcParameters CalculateArcParameters(const Point2D& center,
                                                          const Point2D& start_point,
                                                          const Point2D& end_point,
                                                          ArcInterpolator::ArcDirection direction) const;

    /**
     * @brief 验证圆弧参数
     * @param params 圆弧参数
     * @param config 插补配置
     * @return 验证结果
     */
    bool ValidateArcParameters(const ArcInterpolator::ArcParameters& params, const InterpolationConfig& config) const;

   private:
};

}  // namespace Siligen::Domain::Motion


