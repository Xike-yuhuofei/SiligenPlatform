/**
 * @file SplineGeometryMath.h
 * @brief 样条数学计算工具
 * @details 提供样条插补相关的数学计算功能
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/Point.h"
#include "shared/types/Types.h"
#include "SplineInterpolator.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief 样条数学计算工具类
 * @details 提供样条插补所需的各类数学计算功能
 */
class SplineGeometryMath {
   public:
    /**
     * @brief 构造函数
     */
    SplineGeometryMath() = default;

    /**
     * @brief 析构函数
     */
    ~SplineGeometryMath() = default;

    /**
     * @brief Thomas算法求解三对角矩阵
     * @param a 下对角线
     * @param b 主对角线
     * @param c 上对角线
     * @param d 右侧向量
     * @return 解向量
     */
    std::vector<float32> ThomasAlgorithm(const std::vector<float32>& a,
                                         const std::vector<float32>& b,
                                         const std::vector<float32>& c,
                                         const std::vector<float32>& d) const;

    /**
     * @brief 计算样条曲线导数
     * @param segments 样条段列表
     * @param x x坐标
     * @return 一阶导数
     */
    float32 CalculateSplineDerivative(const std::vector<SplineInterpolator::SplineSegment>& segments, float32 x) const;

    /**
     * @brief 计算样条曲线曲率
     * @param segments 样条段列表
     * @param x x坐标
     * @return 曲率
     */
    float32 CalculateSplineCurvature(const std::vector<SplineInterpolator::SplineSegment>& segments, float32 x) const;

    /**
     * @brief 计算曲线弧长
     * @param segments 样条段列表
     * @param num_samples 采样点数
     * @return 曲线弧长
     */
    float32 CalculateSplineLength(const std::vector<SplineInterpolator::SplineSegment>& segments,
                                  int32 num_samples = 100) const;

    /**
     * @brief 自适应步长控制
     * @param segments 样条段列表
     * @param max_error 最大容许误差
     * @param initial_step 初始步长
     * @return 优化后的步长序列
     */
    std::vector<float32> AdaptiveStepControl(const std::vector<SplineInterpolator::SplineSegment>& segments,
                                             float32 max_error,
                                             float32 initial_step) const;

   private:
};

}  // namespace Siligen::Domain::Motion

