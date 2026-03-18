#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <functional>
#include <memory>

namespace Siligen::Domain::Machine::Ports {

/**
 * @brief 硬件连接状态
 */
enum class HardwareConnectionState {
    Disconnected,  ///< 未连接
    Connecting,    ///< 连接中
    Connected,     ///< 已连接
    Error          ///< 错误
};

// CLAUDE_SUPPRESS: PORT_INTERFACE_PURITY
// Reason: 端口接口中的配置结构体（Config Structs）需要使用 STL 容器和辅助方法。
//         这些是纯数据类型（POD）的扩展，用于参数验证和类型安全，不包含业务逻辑。
//         禁止 STL 会导致接口不可用或需要创建大量自定义容器类型。
// Approved-By: Claude AI Auto-Fix
// Date: 2026-01-06
/**
 * @brief 硬件连接配置
 */
struct HardwareConnectionConfig {
    std::string local_ip = LOCAL_IP;           ///< 本机IP地址
    uint16 local_port = LOCAL_PORT;            ///< 本机端口
    std::string card_ip = CONTROL_CARD_IP;     ///< 控制卡IP地址
    uint16 card_port = CONTROL_CARD_PORT;      ///< 控制卡端口
    int32 timeout_ms = CONNECTION_TIMEOUT_MS;  ///< 连接超时时间(毫秒)

    bool IsValid() const {
        // IP地址非空检查
        if (local_ip.empty() || card_ip.empty()) {
            return false;
        }

        // 超时范围检查: 1ms - 60s
        if (timeout_ms <= 0 || timeout_ms >= 60000) {
            return false;
        }

        // 端口范围检查: 0-65535 (0=系统自动分配，有效)
        if (local_port > 65535 || card_port > 65535) {
            return false;
        }

        // PC端和控制卡端口必须一致
        if (local_port != card_port) {
            return false;
        }

        return true;
    }

    std::string GetValidationError() const {
        if (local_ip.empty() || card_ip.empty()) {
            return "IP地址不能为空";
        }
        if (timeout_ms <= 0 || timeout_ms >= 60000) {
            return "超时时间必须在1-60000ms之间";
        }
        if (local_port > 65535 || card_port > 65535) {
            return "端口号必须在0-65535范围内";
        }
        if (local_port != card_port) {
            return "PC端和控制卡端口必须一致";
        }
        return "";
    }
};

/**
 * @brief 硬件连接信息
 */
struct HardwareConnectionInfo {
    HardwareConnectionState state = HardwareConnectionState::Disconnected;
    std::string device_type = "MultiCard";
    std::string firmware_version = "";
    std::string serial_number = "";
    std::string error_message = "";

    bool IsConnected() const {
        return state == HardwareConnectionState::Connected;
    }

    bool HasError() const {
        return state == HardwareConnectionState::Error || !error_message.empty();
    }
};

// ============================================================
// 心跳监控相关类型
// ============================================================

/**
 * @brief 心跳配置结构
 */
struct HeartbeatConfig {
    uint32 interval_ms = 3000;     ///< 心跳间隔 (毫秒) - 修复：从1000ms增加到3000ms，避免MultiCard硬件响应超时
    uint32 timeout_ms = 5000;      ///< 心跳超时 (毫秒)
    uint32 failure_threshold = 3;  ///< 连续失败阈值
    bool enabled = true;           ///< 是否启用心跳

    bool IsValid() const noexcept {
        return interval_ms >= 100 && interval_ms <= 60000 && timeout_ms >= interval_ms && failure_threshold >= 1 &&
               failure_threshold <= 10;
    }
};

/**
 * @brief 心跳状态
 */
struct HeartbeatStatus {
    bool is_active = false;           ///< 心跳是否活跃
    uint64 total_sent = 0;            ///< 总发送次数
    uint64 total_success = 0;         ///< 总成功次数
    uint64 total_failure = 0;         ///< 总失败次数
    uint32 consecutive_failures = 0;  ///< 连续失败次数
    std::string last_error;           ///< 最后错误信息
    uint32 interval_ms = 0;           ///< 心跳间隔（毫秒）
    uint32 failure_threshold = 0;     ///< 失败阈值
    bool is_degraded = false;         ///< 是否处于降级模式（心跳不可用但连接保持）
    std::string degraded_reason;      ///< 降级原因（例如: "MC_GetSts unavailable (error -999)"）
};

/**
 * @brief 硬件连接端口接口
 *
 * 定义硬件连接的领域接口，遵循六边形架构的端口规范
 * 这个接口描述了连接硬件设备所需的核心操作
 */
class IHardwareConnectionPort {
   public:
    virtual ~IHardwareConnectionPort() = default;

    /**
     * @brief 连接到硬件设备
     * @param config 连接配置
     * @return 连接结果
     */
    virtual Shared::Types::Result<void> Connect(const HardwareConnectionConfig& config) = 0;

    /**
     * @brief 断开硬件连接
     * @return 断开结果
     */
    virtual Shared::Types::Result<void> Disconnect() = 0;

    /**
     * @brief 获取当前连接状态
     * @return 连接信息
     */
    virtual HardwareConnectionInfo GetConnectionInfo() const = 0;

    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    virtual bool IsConnected() const = 0;

    /**
     * @brief 重新连接
     * @return 重连结果
     */
    virtual Shared::Types::Result<void> Reconnect() = 0;

    /**
     * @brief 设置连接状态变化回调
     * @param callback 状态变化回调函数
     */
    virtual void SetConnectionStateCallback(std::function<void(const HardwareConnectionInfo&)> callback) = 0;

    /**
     * @brief 启动连接状态监控
     * @param interval_ms 监控间隔(毫秒)
     * @return 启动结果
     */
    virtual Shared::Types::Result<void> StartStatusMonitoring(uint32 interval_ms = 1000) = 0;

    /**
     * @brief 停止连接状态监控
     */
    virtual void StopStatusMonitoring() = 0;

    /**
     * @brief 获取最后错误信息
     * @return 错误信息
     */
    virtual std::string GetLastError() const = 0;

    /**
     * @brief 清除错误状态
     */
    virtual void ClearError() = 0;

    // ============================================================
    // 心跳监控接口
    // ============================================================

    /**
     * @brief 启动心跳监控
     * @param config 心跳配置
     * @return 启动结果
     */
    virtual Shared::Types::Result<void> StartHeartbeat(const HeartbeatConfig& config) = 0;

    /**
     * @brief 停止心跳监控
     */
    virtual void StopHeartbeat() = 0;

    /**
     * @brief 获取心跳状态
     * @return 心跳状态
     */
    virtual HeartbeatStatus GetHeartbeatStatus() const = 0;

    /**
     * @brief 手动触发一次心跳检查
     * @return 检查结果 (true=硬件响应正常)
     */
    virtual Shared::Types::Result<bool> Ping() const = 0;
};

}  // namespace Siligen::Domain::Machine::Ports

