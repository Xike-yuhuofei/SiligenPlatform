/**
 * @file BSplineCalculator.cpp
 * @brief B样条计算器实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#include "BSplineCalculator.h"

namespace Siligen::Domain::Motion {

float32 BSplineCalculator::BSplineBasis(int32 i, int32 p, float32 t, const std::vector<float32>& knots) const {
    // Cox-de Boor递归公式
    if (p == 0) {
        if (t >= knots[i] && t < knots[i + 1]) {
            return 1.0f;
        } else {
            return 0.0f;
        }
    }

    float32 left = 0.0f, right = 0.0f;

    float32 denom_left = knots[i + p] - knots[i];
    if (denom_left > 1e-6f) {
        left = (t - knots[i]) / denom_left * BSplineBasis(i, p - 1, t, knots);
    }

    float32 denom_right = knots[i + p + 1] - knots[i + 1];
    if (denom_right > 1e-6f) {
        right = (knots[i + p + 1] - t) / denom_right * BSplineBasis(i + 1, p - 1, t, knots);
    }

    return left + right;
}

}  // namespace Siligen::Domain::Motion
