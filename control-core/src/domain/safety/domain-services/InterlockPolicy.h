#pragma once

#include "domain/safety/ports/IInterlockSignalPort.h"
#include "domain/safety/value-objects/InterlockTypes.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

namespace Siligen {
namespace Domain {
namespace Safety {
namespace DomainServices {

using Shared::Types::Result;
using Shared::Types::Error;
using Shared::Types::ErrorCode;
using Ports::IInterlockSignalPort;
using Domain::Motion::Ports::MotionStatus;
using Domain::Motion::Ports::MotionState;
using ValueObjects::InterlockCause;
using ValueObjects::InterlockDecision;
using ValueObjects::InterlockPolicyConfig;
using ValueObjects::InterlockPriority;
using ValueObjects::InterlockSignals;

/**
 * @brief 安全互锁判定策略
 *
 * 仅负责规则判定，不负责采集与动作执行。
 * 通过 Port 获取互锁信号，保持六边形架构依赖方向。
 * 互锁规则唯一入口，其他层不得复写判定逻辑。
 */
class InterlockPolicy {
   public:
    /**
     * @brief 基于信号快照执行互锁判定
     */
    static Result<InterlockDecision> EvaluateSignals(const InterlockSignals& signals,
                                                     const InterlockPolicyConfig& config) noexcept;

    /**
     * @brief 通过端口读取信号并执行互锁判定
     */
    static Result<InterlockDecision> Evaluate(const IInterlockSignalPort& port,
                                              const InterlockPolicyConfig& config) noexcept;

    /**
     * @brief 运动点动互锁检查（急停/硬限位）
     */
    static Result<void> CheckAxisForJog(const MotionStatus& status,
                                        int16_t direction,
                                        const char* error_source = "InterlockPolicy") noexcept;

    /**
     * @brief 回零安全互锁检查（急停/伺服报警/故障等）
     */
    static Result<void> CheckAxisForHoming(const MotionStatus& status,
                                           const char* axis_name,
                                           const char* error_source = "InterlockPolicy") noexcept;

    /**
     * @brief 硬限位触发判定
     */
    static bool IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept;

    /**
     * @brief 软限位触发判定
     */
    static bool IsSoftLimitTriggered(const MotionStatus& status,
                                     bool* positive_limit = nullptr,
                                     bool* negative_limit = nullptr) noexcept;
};

}  // namespace DomainServices
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
