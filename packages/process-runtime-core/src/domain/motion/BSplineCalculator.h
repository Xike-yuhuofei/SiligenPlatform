/**
 * @file BSplineCalculator.h
 * @brief B样条计算器
 * @details 提供B样条基函数计算
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/Types.h"

#include <vector>

namespace Siligen::Domain::Motion {

/**
 * @brief B样条计算器
 * @details 实现B样条基函数的Cox-de Boor递归算法
 */
class BSplineCalculator {
   public:
    /**
     * @brief 构造函数
     */
    BSplineCalculator() = default;

    /**
     * @brief 析构函数
     */
    ~BSplineCalculator() = default;

    /**
     * @brief 计算B样条基函数
     * @param i 基函数索引
     * @param p 阶数
     * @param t 参数值
     * @param knots 节点向量
     * @return 基函数值
     */
    float32 BSplineBasis(int32 i, int32 p, float32 t, const std::vector<float32>& knots) const;

   private:
};

}  // namespace Siligen::Domain::Motion

