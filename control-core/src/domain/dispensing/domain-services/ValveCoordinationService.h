#pragma once

#include "domain/dispensing/ports/IValvePort.h"
#include "domain/dispensing/value-objects/SafetyBoundary.h"
#include "shared/types/Result.h"
#include "shared/types/Error.h"

#include <memory>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::Domain::Dispensing::Ports::DispenserValveParams;
using Siligen::Domain::Dispensing::Ports::PositionTriggeredDispenserParams;
using Siligen::Domain::Dispensing::Ports::DispenserValveState;
using Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Domain::Dispensing::Ports::SupplyValveState;
using Siligen::Domain::Dispensing::Ports::SupplyValveStatusDetail;
using Siligen::Domain::Dispensing::ValueObjects::SafetyBoundary;

/**
 * @brief 阀门协调服务 - Dispensing子域领域服务
 *
 * 职责：
 * - 封装阀门控制的业务逻辑
 * - 参数验证（点胶次数、间隔、持续时间）
 * - 状态管理（供胶阀、点胶阀）
 * - 调用IValvePort接口
 * - 协调供胶阀和点胶阀的联动
 *
 * 约束：
 * - 所有公共方法noexcept
 * - 使用Result<T>错误处理
 * - 无动态内存分配
 * - 无STL容器
 * - 无异常throw
 */
class ValveCoordinationService {
public:
    /**
     * @brief 构造函数
     * @param valve_port Valve控制Port接口
     */
    explicit ValveCoordinationService(std::shared_ptr<IValvePort> valve_port) noexcept;

    // ============================================================
    // 供胶阀控制
    // ============================================================

    /**
     * @brief 打开供胶阀
     * @return Result<SupplyValveState> 成功返回状态，失败返回错误信息
     */
    Result<SupplyValveState> OpenSupplyValve() noexcept;

    /**
     * @brief 关闭供胶阀
     * @return Result<SupplyValveState> 成功返回状态，失败返回错误信息
     */
    Result<SupplyValveState> CloseSupplyValve() noexcept;

    /**
     * @brief 获取供胶阀状态
     * @return Result<SupplyValveStatusDetail> 成功返回状态详情，失败返回错误信息
     */
    Result<SupplyValveStatusDetail> GetSupplyValveStatus() noexcept;

    // ============================================================
    // 点胶阀控制
    // ============================================================

    /**
     * @brief 启动点胶阀（定时触发，阀门单独控制/调试链路）
     * @param params 点胶阀参数配置
     * @return Result<DispenserValveState> 成功返回状态，失败返回错误信息
     */
    Result<DispenserValveState> StartDispenser(const DispenserValveParams& params) noexcept;

    /**
     * @brief 启动位置触发点胶
     * @param params 位置触发点胶参数配置
     * @return Result<DispenserValveState> 成功返回状态，失败返回错误信息
     */
    Result<DispenserValveState> StartPositionTriggeredDispenser(
        const PositionTriggeredDispenserParams& params) noexcept;

    /**
     * @brief 应用安全边界并调整点胶参数（必要时降级）
     * @param params 点胶阀参数
     * @param safety 安全边界
     * @return 成功返回调整后的参数，否则返回错误
     */
    Result<DispenserValveParams> ApplySafetyBoundary(const DispenserValveParams& params,
                                                     const SafetyBoundary& safety) const noexcept;

    /**
     * @brief 停止点胶阀
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> StopDispenser() noexcept;

    /**
     * @brief 暂停点胶阀
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> PauseDispenser() noexcept;

    /**
     * @brief 恢复点胶阀（从暂停状态）
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> ResumeDispenser() noexcept;

    /**
     * @brief 获取点胶阀状态
     * @return Result<DispenserValveState> 成功返回状态，失败返回错误信息
     */
    Result<DispenserValveState> GetDispenserStatus() noexcept;

private:
    std::shared_ptr<IValvePort> valve_port_;

    /**
     * @brief 验证点胶阀参数
     * @param params 点胶阀参数
     * @return Result<void> 验证通过返回Success，否则返回错误信息
     */
    Result<void> ValidateDispenserParameters(const DispenserValveParams& params) const noexcept;

    /**
     * @brief 验证位置触发点胶参数
     * @param params 位置触发点胶参数
     * @return Result<void> 验证通过返回Success，否则返回错误信息
     */
    Result<void> ValidatePositionTriggeredParameters(
        const PositionTriggeredDispenserParams& params) const noexcept;

    /**
     * @brief 检查阀门安全状态
     * @return Result<void> 通过检查返回Success，否则返回错误信息
     */
    Result<void> CheckValveSafety() const noexcept;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices


