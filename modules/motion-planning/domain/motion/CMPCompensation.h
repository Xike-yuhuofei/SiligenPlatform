/**
 * @file CMPCompensation.h
 * @brief CMP触发补偿计算器
 * @details 处理触发位置的延迟补偿和参数调整
 *
 * @author Claude Code
 * @date 2025-11-24
 */

#pragma once

#include "shared/types/CMPTypes.h"
#include "shared/types/Point.h"
#include "shared/types/Types.h"

namespace Siligen::Domain::Motion {

/**
 * @brief CMP补偿计算器
 * @details 负责计算触发位置补偿和动态参数调整
 */
class CMPCompensation {
   public:
    /**
     * @brief 构造函数
     */
    CMPCompensation() = default;

    /**
     * @brief 析构函数
     */
    ~CMPCompensation() = default;

    /**
     * @brief 触发位置处理（软件规划方案）
     * @param current_position 当前位置（已废弃）
     * @param target_position 目标规划位置
     * @param current_velocity 当前速度（已废弃）
     * @param cmp_config CMP配置（已废弃）
     * @return 规划触发位置（直接返回target_position，无补偿）
     * @note 软件方案：使用规划位置+时间戳触发，无需编码器延迟补偿
     */
    Point2D CompensateTriggerPosition(const Point2D& current_position,
                                      const Point2D& target_position,
                                      float32 current_velocity,
                                      const CMPConfiguration& cmp_config) const;

   private:
};

}  // namespace Siligen::Domain::Motion

