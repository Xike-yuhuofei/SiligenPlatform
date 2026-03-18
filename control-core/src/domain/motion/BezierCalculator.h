/**
 * @file BezierCalculator.h
 * @brief Bezier曲线计算器
 * @details 提供Bezier曲线插补算法
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

// CLAUDE_SUPPRESS: DOMAIN_NO_STL_CONTAINERS
// Reason: 贝塞尔曲线插补的轨迹点数量取决于控制点数量、曲线阶数和插补精度参数，
//         运行时大小不可预知，std::vector 是必要的选择。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-07

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "TrajectoryInterpolatorBase.h"
#include "SplineInterpolator.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief Bezier曲线计算器
 * @details 实现Bezier曲线的De Casteljau算法和轨迹生成
 */
class BezierCalculator {
   public:
    /**
     * @brief 构造函数
     */
    BezierCalculator() = default;

    /**
     * @brief 析构函数
     */
    ~BezierCalculator() = default;

    /**
     * @brief De Casteljau算法计算Bezier点
     * @param control_points 控制点
     * @param t 参数值 (0.0-1.0)
     * @return Bezier曲线上的点
     */
    Point2D DeCasteljau(const std::vector<Point2D>& control_points, float32 t) const;

    /**
     * @brief 生成Bezier轨迹点
     * @param control_points Bezier控制点
     * @param config 插补配置
     * @return Bezier轨迹点
     */
    std::vector<TrajectoryPoint> GenerateBezierTrajectory(const SplineInterpolator::BezierControlPoints& control_points,
                                                          const InterpolationConfig& config) const;

   private:
};

}  // namespace Siligen::Domain::Motion


