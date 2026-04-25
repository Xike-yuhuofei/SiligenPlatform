/**
 * @file CMPCompensation.cpp
 * @brief CMP触发补偿计算器实现
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#include "CMPCompensation.h"

#include <cmath>

namespace Siligen::Domain::Motion {

Point2D CMPCompensation::CompensateTriggerPosition(const Point2D& current_position,
                                                   const Point2D& target_position,
                                                   float32 current_velocity,
                                                   const CMPConfiguration& cmp_config) const {
    if (!cmp_config.enable_compensation || current_velocity < 1e-6f) {
        return target_position;
    }

    // 计算延迟补偿距离
    float32 system_delay_ms = cmp_config.time_tolerance_ms;
    float32 compensation_distance = current_velocity * system_delay_ms / 1000.0f;

    // 沿运动方向提前触发
    Point2D direction = (target_position - current_position).Normalized();
    Point2D compensated_position = target_position - direction * compensation_distance * cmp_config.compensation_factor;

    return compensated_position;
}

}  // namespace Siligen::Domain::Motion
