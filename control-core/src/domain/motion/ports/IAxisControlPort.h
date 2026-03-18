#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

// 导入共享类型
using Siligen::Shared::Types::float32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

namespace Siligen::Domain::Motion::Ports {

/**
 * @brief 轴配置参数
 */
struct AxisConfiguration {
    float32 max_velocity = 0.0f;
    float32 max_acceleration = 0.0f;
    float32 jerk = 0.0f;
    float32 following_error_limit = 0.0f;
    float32 in_position_tolerance = 0.0f;
    float32 soft_limit_positive = 0.0f;
    float32 soft_limit_negative = 0.0f;
    bool encoder_enabled = true;
    bool soft_limits_enabled = true;
    bool hard_limits_enabled = true;
};

/**
 * @brief 轴控制端口接口
 * 定义轴的基础控制和参数配置操作
 * 符合接口隔离原则(ISP) - 客户端仅依赖轴控制功能
 */
class IAxisControlPort {
   public:
    virtual ~IAxisControlPort() = default;

    // ========== 轴基础控制 ==========

    /**
     * @brief 轴使能
     * @details 激活指定轴的伺服驱动器，使其可以接收运动指令
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> EnableAxis(LogicalAxisId axis) = 0;

    /**
     * @brief 轴禁止
     * @details 停用指定轴的伺服驱动器，轴将失去保持扭矩
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> DisableAxis(LogicalAxisId axis) = 0;

    /**
     * @brief 检查轴使能状态
     * @details 查询指定轴的伺服驱动器是否已激活
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @return Result<bool> 成功返回轴使能状态，失败返回错误信息
     *         - true 轴已使能
     *         - false 轴未使能
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     */
    virtual Result<bool> IsAxisEnabled(LogicalAxisId axis) const = 0;

    /**
     * @brief 清除轴状态
     * @details 清除指定轴的错误状态和报警标志
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> ClearAxisStatus(LogicalAxisId axis) = 0;

    /**
     * @brief 位置清零
     * @details 将指定轴的当前编码器位置设置为0
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 轴号超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> ClearPosition(LogicalAxisId axis) = 0;

    // ========== 参数设置 ==========

    /**
     * @brief 设置轴速度
     * @details 设置指定轴的最大运行速度
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @param velocity 最大速度 (毫米/秒)，范围 [1, 1000]
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> SetAxisVelocity(LogicalAxisId axis, float32 velocity) = 0;

    /**
     * @brief 设置轴加速度
     * @details 设置指定轴的加速度，影响运动平滑度
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @param acceleration 加速度 (毫米/秒^2)，范围 [100, 10000]
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) = 0;

    /**
     * @brief 设置软限位
     * @details 设置指定轴的软件限位范围，防止超出安全工作区域
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @param negative_limit 负方向软限位 (毫米)
     * @param positive_limit 正方向软限位 (毫米)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围或负限位大于正限位
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) = 0;

    /**
     * @brief 配置轴参数
     * @details 批量设置指定轴的所有配置参数
     * @param axis 轴号 (0-based, 0-3: X=0, Y=1, Z=2, U=3)
     * @param config 轴配置参数结构体
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> ConfigureAxis(LogicalAxisId axis, const AxisConfiguration& config) = 0;

    // ========== 硬限位配置 ==========

    /**
     * @brief 设置硬限位IO通道映射
     * @details 配置指定轴的硬限位信号对应的IO通道
     *          硬限位是物理传感器(EL开关)触发的限位保护
     * @param axis 轴号 (0-based)
     * @param positive_io_index 正向限位IO通道索引 (0-31)
     * @param negative_io_index 负向限位IO通道索引 (0-31)
     * @param card_index 卡号索引 (默认0)
     * @param signal_type 信号类型: 0=常开, 1=常闭 (默认0)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> SetHardLimits(LogicalAxisId axis,
                                       short positive_io_index,
                                       short negative_io_index,
                                       short card_index = 0,
                                       short signal_type = 0) = 0;

    /**
     * @brief 启用或禁用硬限位
     * @details 控制指定轴的硬限位功能开关
     * @param axis 轴号 (0-based)
     * @param enable true=启用, false=禁用
     * @param limit_type 限位类型: -1=正负向都操作, 0=正向, 1=负向 (默认-1)
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type = -1) = 0;

    /**
     * @brief 设置硬限位信号极性
     * @details 配置指定轴的正负向限位信号的有效电平
     * @param axis 轴号 (0-based)
     * @param positive_polarity 正向限位极性: 0=低电平有效, 1=高电平有效
     * @param negative_polarity 负向限位极性: 0=低电平有效, 1=高电平有效
     * @return Result<void> 成功返回Success，失败返回错误信息
     *         - ErrorCode::INVALID_PARAMETER 参数超出范围
     *         - ErrorCode::HARDWARE_OPERATION_FAILED 硬件操作失败
     */
    virtual Result<void> SetHardLimitPolarity(LogicalAxisId axis,
                                              short positive_polarity,
                                              short negative_polarity) = 0;
};

}  // namespace Siligen::Domain::Motion::Ports
