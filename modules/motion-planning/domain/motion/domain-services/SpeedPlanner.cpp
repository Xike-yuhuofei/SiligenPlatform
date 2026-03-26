#include "SpeedPlanner.h"

#include <algorithm>
#include <cmath>

namespace Siligen::Domain::Motion {

namespace {
constexpr float32 kEpsilon = 1e-5f;
}  // namespace

SpeedPlan SpeedPlanner::Plan(const std::vector<MotionSegment>& segments, const InterpolationConfig& config) const {
    SpeedPlan plan;
    plan.max_velocities.resize(segments.size(), config.max_velocity);

    if (segments.empty()) {
        return plan;
    }

    // 曲率限速
    for (size_t i = 0; i < segments.size(); ++i) {
        float32 limit = ComputeCurvatureLimit(segments[i], config);
        plan.max_velocities[i] = std::min(config.max_velocity, limit);
    }

    // 前向约束 (加速度限制)
    for (size_t i = 1; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        float32 max_speed = std::sqrt(plan.max_velocities[i - 1] * plan.max_velocities[i - 1] +
                                      2.0f * seg.max_acceleration * std::max(seg.length, kEpsilon));
        plan.max_velocities[i] = std::min(plan.max_velocities[i], max_speed);
    }

    // 后向约束 (减速度限制)
    for (int32 i = static_cast<int32>(segments.size()) - 2; i >= 0; --i) {
        const auto& seg = segments[i];
        float32 max_speed = std::sqrt(plan.max_velocities[i + 1] * plan.max_velocities[i + 1] +
                                      2.0f * seg.max_acceleration * std::max(seg.length, kEpsilon));
        plan.max_velocities[i] = std::min(plan.max_velocities[i], max_speed);
    }

    return plan;
}

float32 SpeedPlanner::ComputeCurvatureLimit(const MotionSegment& segment, const InterpolationConfig& config) const {
    if (segment.curvature_radius <= kEpsilon) {
        return config.max_velocity;
    }

    float32 scaled_factor = std::max(0.1f, config.curvature_speed_factor);
    float32 limit = std::sqrt(std::max(0.0f, segment.max_acceleration * segment.curvature_radius));
    return limit * scaled_factor;
}

}  // namespace Siligen::Domain::Motion
