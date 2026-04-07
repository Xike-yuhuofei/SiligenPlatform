#pragma once

#include "domain/motion/ports/IJogControlPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Result.h"
#include "shared/types/Error.h"

#include <memory>

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Domain::Motion::Ports::IJogControlPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;

/**
 * @brief Jog运动状态
 */
enum class JogState : uint8_t {
    Idle = 0,      // 空闲
    Jogging = 1,   // 点动中
    Stopping = 2,  // 停止中
    Fault = 3      // 故障
};

/**
 * @brief Jog控制Domain Service
 *
 * 职责：
 * - 封装Jog运动的业务逻辑
 * - 安全互锁检查（统一委托 Safety::InterlockPolicy）
 * - 参数验证（速度范围、轴号有效性）
 * - 调用IJogControlPort接口
 * - 点动规则唯一入口（上层不得绕过或复写点动规则）
 *
 * 约束：
 * - 所有公共方法noexcept
 * - 使用Result<T>错误处理
 * - 无动态内存分配
 * - 无STL容器
 * - 无异常throw
 */
class JogController {
public:
    /**
     * @brief 构造函数
     * @param jog_port Jog控制Port接口
     * @param state_port 运动状态查询Port接口（用于安全互锁检查）
     */
    explicit JogController(std::shared_ptr<IJogControlPort> jog_port,
                          std::shared_ptr<IMotionStatePort> state_port) noexcept;

    /**
     * @brief 启动连续点动
     * @param axis 逻辑轴号 (0-based)
     * @param direction 方向 (1=正向, -1=负向)
     * @param velocity 运动速度 (mm/s)
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> StartContinuousJog(LogicalAxisId axis, int16_t direction, float velocity) noexcept;

    /**
     * @brief 启动步进点动
     * @param axis 逻辑轴号 (0-based)
     * @param direction 方向 (1=正向, -1=负向)
     * @param distance 运动距离 (mm)
     * @param velocity 运动速度 (mm/s)
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> StartStepJog(LogicalAxisId axis, int16_t direction, float distance, float velocity) noexcept;

    /**
     * @brief 停止点动
     * @param axis 逻辑轴号 (0-based)
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> StopJog(LogicalAxisId axis) noexcept;

    /**
     * @brief 获取点动状态
     * @param axis 逻辑轴号 (0-based)
     * @return Result<JogState> 成功返回状态，失败返回错误信息
     */
    Result<JogState> GetJogState(LogicalAxisId axis) const noexcept;

private:
    std::shared_ptr<IJogControlPort> jog_port_;
    std::shared_ptr<IMotionStatePort> state_port_;  // 状态查询Port，用于安全互锁检查

    /**
     * @brief 安全互锁检查
     * @param axis 轴号
     * @param direction 方向
     * @return Result<void> 通过检查返回Success，否则返回错误信息
     */
    Result<void> CheckSafetyInterlocks(LogicalAxisId axis, int16_t direction) const noexcept;

    /**
     * @brief 参数验证
     * @param axis 轴号
     * @param velocity 速度
     * @return Result<void> 验证通过返回Success，否则返回错误信息
     */
    Result<void> ValidateJogParameters(LogicalAxisId axis, float velocity) const noexcept;

    /**
     * @brief 验证距离参数
     * @param distance 距离
     * @return Result<void> 验证通过返回Success，否则返回错误信息
     */
    Result<void> ValidateDistance(float distance) const noexcept;
};

}  // namespace Siligen::Domain::Motion::DomainServices

