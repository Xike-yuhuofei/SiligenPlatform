#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Siligen {
namespace Domain {
namespace Dispensing {
namespace Ports {

using Siligen::Shared::Types::LogicalAxisId;

// CLAUDE_SUPPRESS: PORT_INTERFACE_PURITY
// Reason: 端口接口中的配置结构体（Config Structs）需要使用 STL 容器和辅助方法。
//         这些是纯数据类型（POD）的扩展，用于参数验证和类型安全，不包含业务逻辑。
//         禁止 STL 会导致接口不可用或需要创建大量自定义容器类型。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-06
/**
 * @brief 点胶阀参数配置（定时触发，阀门单独控制/调试链路）
 */
struct DispenserValveParams {
    uint32 count = 1;          ///< 点胶次数 (1-1000)
    uint32 intervalMs = 1000;  ///< 点胶间隔 (10-60000 ms)
    uint32 durationMs = 100;   ///< 单次持续时间 (10-5000 ms)

    /**
     * @brief 验证参数有效性
     * @return true 如果参数有效
     */
    bool IsValid() const noexcept {
        return (count >= 1 && count <= 1000) && (intervalMs >= 10 && intervalMs <= 60000) &&
               (durationMs >= 10 && durationMs <= 5000);
    }

    /**
     * @brief 获取参数验证错误信息
     * @return 错误信息，如果参数有效则返回空字符串
     */
    std::string GetValidationError() const {
        if (count < 1 || count > 1000) {
            return "点胶次数必须在1-1000范围内";
        }
        if (intervalMs < 10 || intervalMs > 60000) {
            return "点胶间隔必须在10-60000ms范围内";
        }
        if (durationMs < 10 || durationMs > 5000) {
            return "单次持续时间必须在10-5000ms范围内";
        }
        return "";
    }

    bool operator==(const DispenserValveParams& other) const noexcept {
        return count == other.count &&
               intervalMs == other.intervalMs &&
               durationMs == other.durationMs;
    }
};

/**
 * @brief 位置触发点胶参数配置
 *
 * 用于基于轴位置触发的点胶控制，适用于沿路径点胶的场景。
 * 通过 MC_CmpBufData 加载位置数组到 CMP 缓冲区，运动时自动触发。
 *
 * @note 轴号约定: 0-based (X=0, Y=1, Z=2, U=3)
 *       Infrastructure层的ValveAdapter负责转换为SDK的1-based轴号
 */
struct PositionTriggeredDispenserParams {
    static constexpr size_t kMaxTriggerPositions = 100000;
    std::vector<long> trigger_positions;  ///< 触发位置数组（脉冲单位，1-kMaxTriggerPositions 个位置）
    LogicalAxisId axis = LogicalAxisId::X;  ///< 触发轴号（0-based: X=0, Y=1, Z=2, U=3）
    uint32 pulse_width_ms = 100;          ///< 脉冲宽度（10-5000 ms）
    short start_level = 0;                ///< 起始电平（0=低电平，1=高电平）

    /**
     * @brief 验证参数有效性
     * @return true 如果参数有效
     */
    bool IsValid() const noexcept {
        return !trigger_positions.empty() && trigger_positions.size() <= kMaxTriggerPositions &&
               Siligen::Shared::Types::IsValid(axis) &&
               pulse_width_ms >= 10 && pulse_width_ms <= 5000 && (start_level == 0 || start_level == 1);
    }

    /**
     * @brief 获取参数验证错误信息
     * @return 错误信息，如果参数有效则返回空字符串
     */
    std::string GetValidationError() const {
        if (trigger_positions.empty()) {
            return "触发位置数组不能为空";
        }
        if (trigger_positions.size() > kMaxTriggerPositions) {
            return "触发位置数组长度不能超过" + std::to_string(kMaxTriggerPositions);
        }
        if (!Siligen::Shared::Types::IsValid(axis)) {
            return "轴号必须为有效逻辑轴 (0-based: X=0, Y=1, Z=2, U=3)";
        }
        if (pulse_width_ms < 10 || pulse_width_ms > 5000) {
            return "脉冲宽度必须在10-5000ms范围内";
        }
        if (start_level != 0 && start_level != 1) {
            return "起始电平必须为0（低电平）或1（高电平）";
        }
        return "";
    }

    bool operator==(const PositionTriggeredDispenserParams& other) const noexcept {
        return trigger_positions == other.trigger_positions &&
               axis == other.axis &&
               pulse_width_ms == other.pulse_width_ms &&
               start_level == other.start_level;
    }
};

/**
 * @brief 点胶阀状态枚举
 */
enum class DispenserValveStatus {
    Idle,     ///< 空闲状态
    Running,  ///< 运行中
    Paused,   ///< 已暂停
    Stopped,  ///< 已停止
    Error     ///< 错误状态
};

/**
 * @brief 点胶阀状态信息
 */
struct DispenserValveState {
    DispenserValveStatus status = DispenserValveStatus::Idle;  ///< 当前状态
    uint32 completedCount = 0;                                 ///< 已完成次数
    uint32 totalCount = 0;                                     ///< 总次数
    uint32 remainingCount = 0;                                 ///< 剩余次数
    float progress = 0.0f;                                     ///< 进度百分比 (0-100)
    std::optional<uint64> startTime;                           ///< 启动时间戳 (毫秒)
    std::string errorMessage;                                  ///< 错误信息

    /**
     * @brief 检查是否处于运行状态
     * @return true 如果正在运行
     */
    bool IsRunning() const noexcept {
        return status == DispenserValveStatus::Running;
    }

    /**
     * @brief 检查是否已完成
     * @return true 如果已完成所有点胶操作
     */
    bool IsCompleted() const noexcept {
        return status == DispenserValveStatus::Idle && completedCount == totalCount && totalCount > 0;
    }

    /**
     * @brief 检查是否有错误
     * @return true 如果处于错误状态
     */
    bool HasError() const noexcept {
        return status == DispenserValveStatus::Error || !errorMessage.empty();
    }
};

/**
 * @brief 供胶阀状态枚举
 */
enum class SupplyValveState {
    Open,    ///< 打开状态
    Closed,  ///< 关闭状态
    Error    ///< 错误状态
};

/**
 * @brief 供胶阀状态详细信息
 */
struct SupplyValveStatusDetail {
    SupplyValveState state = SupplyValveState::Closed;  ///< 当前状态
    std::optional<uint64> lastChangeTime;               ///< 最后状态变更时间戳 (毫秒)
    std::string errorMessage;                           ///< 错误信息

    /**
     * @brief 检查是否打开
     * @return true 如果阀门打开
     */
    bool IsOpen() const noexcept {
        return state == SupplyValveState::Open;
    }

    /**
     * @brief 检查是否有错误
     * @return true 如果处于错误状态
     */
    bool HasError() const noexcept {
        return state == SupplyValveState::Error || !errorMessage.empty();
    }
};

/**
 * @brief 阀门控制端口接口
 *
 * 定义阀门控制的领域接口，遵循六边形架构的端口规范
 * 包含点胶阀（分液阀）和供胶阀的控制操作
 */
class IValvePort {
   public:
    virtual ~IValvePort() = default;

    // ============================================================
    // 点胶阀（分液阀）控制接口
    // ============================================================

    /**
     * @brief 启动点胶阀（定时触发，阀门单独控制/调试链路）
     * @param params 点胶阀参数配置
     * @return 启动后的点胶阀状态，失败则返回错误
     */
    virtual Shared::Types::Result<DispenserValveState> StartDispenser(const DispenserValveParams& params) noexcept = 0;

    /**
     * @brief 连续打开点胶阀（保持电平）
     * @return 点胶阀状态，失败则返回错误
     */
    virtual Shared::Types::Result<DispenserValveState> OpenDispenser() noexcept = 0;

    /**
     * @brief 关闭点胶阀（结束连续出胶）
     * @return 成功返回 void，失败则返回错误
     */
    virtual Shared::Types::Result<void> CloseDispenser() noexcept = 0;

    /**
     * @brief 启动位置触发点胶
     *
     * 基于轴位置触发点胶，适用于沿路径点胶的场景。
     * 通过 MC_CmpBufData 加载位置数组到硬件 CMP 缓冲区，
     * 当运动轴到达指定位置时自动触发点胶阀输出脉冲。
     *
     * @param params 位置触发点胶参数配置
     * @return 启动后的点胶阀状态，失败则返回错误
     *
     * @note 在调用此方法前，应先调用 MC_EncOff 关闭编码器反馈，
     *       切换 CMP 比较源为规划位置（Profile Position），
     *       以保证非匀速运动时触发位置的均匀性。
     */
    virtual Shared::Types::Result<DispenserValveState> StartPositionTriggeredDispenser(
        const PositionTriggeredDispenserParams& params) noexcept = 0;

    /**
     * @brief 停止点胶阀
     * @return 成功返回 void，失败则返回错误
     */
    virtual Shared::Types::Result<void> StopDispenser() noexcept = 0;

    /**
     * @brief 暂停点胶阀
     * @return 成功返回 void，失败则返回错误
     */
    virtual Shared::Types::Result<void> PauseDispenser() noexcept = 0;

    /**
     * @brief 恢复点胶阀（从暂停状态）
     * @return 成功返回 void，失败则返回错误
     */
    virtual Shared::Types::Result<void> ResumeDispenser() noexcept = 0;

    /**
     * @brief 获取点胶阀当前状态
     * @return 点胶阀状态信息，失败则返回错误
     */
    virtual Shared::Types::Result<DispenserValveState> GetDispenserStatus() noexcept = 0;

    // ============================================================
    // 供胶阀控制接口
    // ============================================================

    /**
     * @brief 打开供胶阀
     * @return 供胶阀状态，失败则返回错误
     */
    virtual Shared::Types::Result<SupplyValveState> OpenSupply() noexcept = 0;

    /**
     * @brief 关闭供胶阀
     * @return 供胶阀状态，失败则返回错误
     */
    virtual Shared::Types::Result<SupplyValveState> CloseSupply() noexcept = 0;

    /**
     * @brief 获取供���阀当前状态
     * @return 供胶阀详细状态，失败则返回错误
     */
    virtual Shared::Types::Result<SupplyValveStatusDetail> GetSupplyStatus() noexcept = 0;
};

}  // namespace Ports
}  // namespace Dispensing
}  // namespace Domain
}  // namespace Siligen

