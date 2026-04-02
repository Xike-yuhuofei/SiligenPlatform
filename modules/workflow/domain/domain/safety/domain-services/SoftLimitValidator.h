#pragma once

#include "shared/types/Point.h"
#include "shared/types/Result.h"
#include "../../configuration/value-objects/ConfigTypes.h"

#include <cstddef>
#include <cstdint>

namespace Siligen {
namespace Domain {
namespace Safety {
namespace DomainServices {

// 导入类型
using Shared::Types::float32;
using Shared::Types::Result;
using Siligen::TrajectoryPoint;
using Siligen::Domain::Configuration::ValueObjects::AxisConfiguration;
using Siligen::Domain::Configuration::ValueObjects::MotionConfiguration;

/**
 * @brief 软限位验证器
 * 
 * 验证轨迹点是否超出配置的软限位范围。
 * 使用C风格API（指针+长度）以避免Domain层STL容器违规。
 * 
 * ## 架构合规性
 * - ✅ DOMAIN_NO_STL_CONTAINERS: 使用指针+长度而非std::vector
 * - ✅ DOMAIN_NO_DYNAMIC_MEMORY: 无动态分配
 * - ✅ DOMAIN_PUBLIC_API_NOEXCEPT: 所有公共方法为noexcept
 * 
 * ## 浮点精度处理
 * - 使用EPSILON=1e-6f进行浮点比较
 * - 封装FloatLessOrEqual/FloatGreaterOrEqual/FloatInRange辅助函数
 * - 处理累积误差和单位转换误差
 */
class SoftLimitValidator {
   public:
    /**
     * @brief 验证轨迹点数组是否超出软限位
     * 
     * @param trajectory 轨迹点数组指针
     * @param count 轨迹点数量
     * @param config 机器配置
     * @return Result<void> 成功返回Success，失败返回具体错误
     * 
     * @note C风格API设计，避免Domain层使用std::vector
     * @note 空轨迹（trajectory==nullptr 或 count==0）视为有效
     * 
     * ## 错误代码
     * - INVALID_PARAMETER: trajectory==nullptr且count>0
     * - CONFIG_ERROR: 配置无效（min>=max, NaN, Inf）
     * - OUT_OF_RANGE: 轨迹点超出软限位
     */
    static Result<void> ValidateTrajectory(
        const TrajectoryPoint* trajectory,
        size_t count,
        const MotionConfiguration& config) noexcept;

    /**
     * @brief 验证轨迹点数组（支持轴级别启用/禁用）
     * 
     * @param trajectory 轨迹点数组指针
     * @param count 轨迹点数量
     * @param config 机器配置
     * @param axis_configs 轴配置数组（用于软限位启用/禁用控制）
     * @param axis_count 轴配置数量
     * @return Result<void> 成功返回Success，失败返回具体错误
     * 
     * @note 支持按轴禁用软限位验证（axis.softLimits.enabled==false）
     */
    static Result<void> ValidateTrajectory(
        const TrajectoryPoint* trajectory,
        size_t count,
        const MotionConfiguration& config,
        const AxisConfiguration* axis_configs,
        size_t axis_count) noexcept;

    /**
     * @brief 验证单个点是否在软限位范围内
     * 
     * @param point 待验证的2D点
     * @param config 机器配置
     * @return Result<void> 成功返回Success，失败返回具体错误
     */
    static Result<void> ValidatePoint(
        const Shared::Types::Point2D& point,
        const MotionConfiguration& config) noexcept;

   private:
    // 浮点比较EPSILON常量
    static constexpr float32 EPSILON = 1e-6f;

    /**
     * @brief 浮点小于等于比较（带EPSILON容差）
     * 
     * @param a 左操作数
     * @param b 右操作数
     * @return true a <= b + EPSILON
     */
    static bool FloatLessOrEqual(float32 a, float32 b) noexcept;

    /**
     * @brief 浮点大于等于比较（带EPSILON容差）
     * 
     * @param a 左操作数
     * @param b 右操作数
     * @return true a >= b - EPSILON
     */
    static bool FloatGreaterOrEqual(float32 a, float32 b) noexcept;

    /**
     * @brief 浮点范围检查（带EPSILON容差）
     * 
     * @param val 待检查值
     * @param min 范围最小值
     * @param max 范围最大值
     * @return true min <= val <= max（考虑EPSILON）
     */
    static bool FloatInRange(float32 val, float32 min, float32 max) noexcept;

    /**
     * @brief 验证浮点数是否为有限值（非NaN非Inf）
     * 
     * @param value 待检查值
     * @return true 值是有限的
     */
    static bool IsFinite(float32 value) noexcept;

    /**
     * @brief 验证软限位配置有效性
     * 
     * @param config 机器配置
     * @return Result<void> 配置有效返回Success
     * 
     * ## 检查项
     * - min < max（对于所有启用的软限位）
     * - 无NaN或Inf值
     */
    static Result<void> ValidateConfig(const MotionConfiguration& config) noexcept;

    /**
     * @brief 验证单个轴的软限位配置
     * 
     * @param axis_id 轴ID（用于错误消息）
     * @param min_val 最小值
     * @param max_val 最大值
     * @param enabled 是否启用
     * @return Result<void> 配置有效返回Success
     */
    static Result<void> ValidateAxisLimit(
        Siligen::Shared::Types::LogicalAxisId axis_id,
        float32 min_val,
        float32 max_val,
        bool enabled) noexcept;
};

}  // namespace DomainServices
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen

