#pragma once

#include "shared/types/Point.h"

#include <vector>

namespace Siligen::Domain::Motion {

struct SpeedPlan {
    std::vector<float32> max_velocities;
};

/**
 * @brief 速度规划器
 * @details 根据曲率与加速度约束输出分段限速
 */
class SpeedPlanner {
   public:
    SpeedPlanner() = default;
    ~SpeedPlanner() = default;

    SpeedPlan Plan(const std::vector<MotionSegment>& segments, const InterpolationConfig& config) const;

   private:
    float32 ComputeCurvatureLimit(const MotionSegment& segment, const InterpolationConfig& config) const;
};

}  // namespace Siligen::Domain::Motion
