/**
 * HardwareConnectionAdapter.cpp - 硬件连接适配器实现
 */

// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "HardwareConnection"

#include "HardwareConnectionAdapter.h"

#include "modules/device-hal/src/drivers/multicard/MultiCardAdapter.h"
#include "shared/interfaces/ILoggingService.h"

#include <chrono>
#include <sstream>
#include <thread>

// 明确取消定义Windows宏以避免污染
#ifdef GetMessage
#undef GetMessage
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace {
constexpr auto kConnectionGracePeriod = std::chrono::milliseconds(7000);

bool IsGracePeriodActive(const std::chrono::steady_clock::time_point& last_connect_time) {
    if (last_connect_time == std::chrono::steady_clock::time_point{}) {
        return false;
    }
    return (std::chrono::steady_clock::now() - last_connect_time) < kConnectionGracePeriod;
}
}  // namespace

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

HardwareConnectionAdapter::HardwareConnectionAdapter(
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
    Siligen::Shared::Types::HardwareMode mode,
    const Siligen::Shared::Types::HardwareConfiguration& hardware_config)
    : connection_state_(Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected),
      last_error_(),
      connection_info_(),
      hardware_handle_(nullptr),
      multicard_adapter_(),
      shared_multicard_(multicard),
      hardware_mode_(mode),
      hardware_config_(hardware_config)
// monitoring_thread_, should_monitor_, monitoring_active_ 有默认构造/就地初始化
// heartbeat_thread_, should_heartbeat_, heartbeat_active_, heartbeat_mutex_ 有默认构造/就地初始化
// heartbeat_config_, heartbeat_status_ 有默认构造
// state_change_callback_ 有默认构造
{
    // 确保所有原子变量都正确初始化
    should_monitor_ = false;
    monitoring_active_ = false;
    should_heartbeat_ = false;
    heartbeat_active_ = false;
}

HardwareConnectionAdapter::~HardwareConnectionAdapter() {
    Disconnect();
    StopStatusMonitoring();
    StopHeartbeat();  // 停止心跳
}

Shared::Types::Result<void> HardwareConnectionAdapter::Connect(const Siligen::Domain::Machine::Ports::HardwareConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Connecting) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE, "正在连接中，请勿重复操作"));
    }

    if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected) {
        return Shared::Types::Result<void>::Success();  // 已经连接
    }

    // 验证配置
    if (!config.IsValid()) {
        std::string error_msg = "连接配置无效: " + config.GetValidationError();
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER, error_msg));
    }

    connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Connecting;
    last_error_.clear();

    try {
        // 创建MultiCard适配器 (传入共享的IMultiCardWrapper实例)
        // 注意：不再需要传递硬件模式，因为wrapper已经包含了模式信息
        const auto& hw_config_obj = hardware_config_;
        auto multicard_adapter =
            std::make_unique<Siligen::Infrastructure::Hardware::MultiCardAdapter>(shared_multicard_, hw_config_obj);

        // 将HardwareConnectionConfig转换为ConnectionConfig
        Shared::Types::ConnectionConfig conn_config;
        conn_config.local_ip = config.local_ip;
        conn_config.card_ip = config.card_ip;
        conn_config.port = config.card_port;
        conn_config.timeout_ms = config.timeout_ms;

        // 使用MultiCard适配器建立连接
        auto connect_result = multicard_adapter->Connect(conn_config);

        if (connect_result.IsSuccess()) {
            // 保存适配器
            multicard_adapter_ = std::move(multicard_adapter);
            hardware_handle_ = multicard_adapter_.get();  // 使用真实适配器指针作为句柄

            // 等待硬件完全稳定后再验证（额外200ms）
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // WORKAROUND: MC_GetSts在某些硬件上初始化后立即调用会返回-999
            // 由于MC_Open、MC_Reset和轴使能都已成功，我们信任连接已建立
            // 后续的心跳检测会持续验证硬件状态
            SILIGEN_LOG_INFO("Hardware connection established (MC_Open, MC_Reset, AxisOn succeeded)");

            // 更新连接状态
            connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected;
            last_connect_time_ = std::chrono::steady_clock::now();

            // 获取设备信息
            UpdateDeviceInfo();

            return Shared::Types::Result<void>::Success();
        } else {
            connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Error;
            last_error_ = connect_result.GetError().GetMessage();

            return Shared::Types::Result<void>::Failure(
                Shared::Types::Error(Shared::Types::ErrorCode::CONNECTION_FAILED, last_error_));
        }

    } catch (const std::exception& e) {
        connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Error;
        last_error_ = std::string("连接异常: ") + e.what();

        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_CONNECTION_FAILED, last_error_));
    }
}

Shared::Types::Result<void> HardwareConnectionAdapter::Disconnect() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected) {
        return Shared::Types::Result<void>::Success();  // 已经断开
    }

    try {
        // 断开MultiCard连接
        if (multicard_adapter_) {
            multicard_adapter_->Disconnect();
            multicard_adapter_.reset();
        }

        hardware_handle_ = nullptr;
        connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected;
        last_error_.clear();
        last_connect_time_ = std::chrono::steady_clock::time_point{};

        // 清除设备信息
        connection_info_.device_type = "MultiCard";
        connection_info_.firmware_version = "";
        connection_info_.serial_number = "";

        return Shared::Types::Result<void>::Success();

    } catch (const std::exception& e) {
        last_error_ = std::string("断开异常: ") + e.what();
        connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Error;

        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_CONNECTION_FAILED, last_error_));
    }
}

Siligen::Domain::Machine::Ports::HardwareConnectionInfo HardwareConnectionAdapter::GetConnectionInfo() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    // 调试日志：记录当前软件状态
    auto state_str = std::string(connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected ? "Connected" : "Disconnected");
    SILIGEN_LOG_DEBUG(std::string("Current software state: ") + state_str);

    // CRITICAL FIX: 在返回状态前验证硬件连接
    // 但在降级模式下，跳过 MC_GetSts 验证（因为已知它不可用）
    if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected) {
        // 检查是否处于降级模式
        std::lock_guard<std::mutex> hb_lock(heartbeat_mutex_);
        bool is_degraded = heartbeat_status_.is_degraded;
        bool in_grace_period = IsGracePeriodActive(last_connect_time_);

        if (is_degraded || in_grace_period) {
            // 降级模式: 跳过 MC_GetSts 验证，信任连接状态
            if (is_degraded) {
                SILIGEN_LOG_DEBUG("Degraded mode: skipping MC_GetSts verification");
                SILIGEN_LOG_DEBUG(std::string("Reason: ") + heartbeat_status_.degraded_reason);
            } else {
                SILIGEN_LOG_DEBUG("Grace period active: skipping MC_GetSts verification");
            }
            // 不修改 connection_state_，保持 Connected
        } else {
            // 正常模式: 验证硬件响应
            if (multicard_adapter_) {
                SILIGEN_LOG_DEBUG("Verifying hardware response...");

                auto result = multicard_adapter_->IsConnected();
                if (!result.IsSuccess() || !result.Value()) {
                    std::string error_msg = result.IsError() ? result.GetError().GetMessage() : "No response";
                    SILIGEN_LOG_WARNING(std::string("Hardware not responding! Error: ") + error_msg);

                    // 硬件未响应 - 更新状态
                    const_cast<HardwareConnectionAdapter*>(this)->connection_state_ =
                        Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected;
                    const_cast<HardwareConnectionAdapter*>(this)->last_error_ =
                        "Hardware not responding (power off or connection lost)";
                } else {
                    SILIGEN_LOG_DEBUG("Hardware verified responding");
                }
            }
        }
    }

    Siligen::Domain::Machine::Ports::HardwareConnectionInfo info = connection_info_;
    info.state = connection_state_;
    info.error_message = last_error_;

    // 调试日志：返回状态
    SILIGEN_LOG_DEBUG(std::string("Returning state: ") + (info.IsConnected() ? "Connected" : "Disconnected") +
                      ", Error: " + info.error_message);

    return info;
}

bool HardwareConnectionAdapter::IsConnected() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    // Quick check: if software state is not connected, return false
    if (connection_state_ != Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected) {
        return false;
    }

    if (IsGracePeriodActive(last_connect_time_)) {
        return true;
    }

    // Active hardware check: verify actual hardware response
    if (multicard_adapter_) {
        auto result = multicard_adapter_->IsConnected();
        if (!result.IsSuccess() || !result.Value()) {
            // Hardware not responding - update state
            const_cast<HardwareConnectionAdapter*>(this)->connection_state_ =
                Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected;
            const_cast<HardwareConnectionAdapter*>(this)->last_error_ = "Hardware not responding";
            return false;
        }
    }

    return true;
}

Shared::Types::Result<void> HardwareConnectionAdapter::Reconnect() {
    auto disconnect_result = Disconnect();
    if (disconnect_result.IsError()) {
        return disconnect_result;
    }

    // 等待一小段时间再重连
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 使用当前配置重连（这里简化处理，实际应该保存配置）
    Siligen::Domain::Machine::Ports::HardwareConnectionConfig config;
    return Connect(config);
}

void HardwareConnectionAdapter::SetConnectionStateCallback(
    std::function<void(const Siligen::Domain::Machine::Ports::HardwareConnectionInfo&)> callback) {
    state_change_callback_ = std::move(callback);
}

Shared::Types::Result<void> HardwareConnectionAdapter::StartStatusMonitoring(uint32 interval_ms) {
    if (monitoring_active_) {
        return Shared::Types::Result<void>::Success();  // 已经在监控
    }

    should_monitor_ = true;
    monitoring_active_ = true;

    // 启动监控线程
    monitoring_thread_ = std::thread([this, interval_ms]() { MonitoringLoop(interval_ms); });

    return Shared::Types::Result<void>::Success();
}

void HardwareConnectionAdapter::StopStatusMonitoring() {
    should_monitor_ = false;

    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    monitoring_active_ = false;
}

std::string HardwareConnectionAdapter::GetLastError() const {
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return last_error_;
}

void HardwareConnectionAdapter::ClearError() {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Error) {
        connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected;
    }

    last_error_.clear();
    connection_info_.error_message.clear();
}

void HardwareConnectionAdapter::MonitoringLoop(uint32 interval_ms) {
    Siligen::Domain::Machine::Ports::HardwareConnectionState last_state = connection_state_;

    while (should_monitor_) {
        try {
            // 检查实际硬件状态（这里简化处理）
            Siligen::Domain::Machine::Ports::HardwareConnectionState current_state = connection_state_;

            // 如果有适配器，检查实际连接状态
            if (multicard_adapter_ && connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected) {
                auto connected_result = multicard_adapter_->IsConnected();
                bool is_connected = connected_result.IsSuccess() && connected_result.Value();
                if (!is_connected) {
                    current_state = Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected;
                    last_error_ = "连接意外断开";
                    connection_state_ = current_state;
                }
            }

            // 状态变化检测
            if (last_state != current_state && state_change_callback_) {
                connection_info_.state = current_state;
                connection_info_.error_message = last_error_;
                state_change_callback_(connection_info_);
            }

            last_state = current_state;

        } catch (const std::exception& e) {
            last_error_ = std::string("监控异常: ") + e.what();
            connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Error;
        }

        // 等待下一次检查
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

void HardwareConnectionAdapter::UpdateDeviceInfo() {
    // 设置基本设备类型
    connection_info_.device_type = "MultiCard Motion Controller";

    // TODO: 从硬件获取真实设备信息
    // MultiCard SDK 可能提供以下API获取设备信息：
    // - MC_GetVersion() - 获取固件版本
    // - MC_GetCardInfo() - 获取控制卡信息
    // - MC_GetSerialNumber() - 获取序列号
    //
    // 当前限制：由于 MultiCard SDK 文档未明确这些API的存在性，
    // 暂时使用默认值。如果连接成功，说明硬件通信正常。

    if (multicard_adapter_) {
        // 连接成功时设置状态信息
        connection_info_.firmware_version = "Unknown";  // 待从硬件读取
        connection_info_.serial_number = "Unknown";     // 待从硬件读取
    } else {
        connection_info_.firmware_version = "";
        connection_info_.serial_number = "";
    }
}


// ============================================================
// 心跳监控实现
// ============================================================

Shared::Types::Result<void> HardwareConnectionAdapter::StartHeartbeat(const Siligen::Domain::Machine::Ports::HeartbeatConfig& config) {
    std::lock_guard<std::mutex> lock(connection_mutex_);

    // 验证配置
    if (!config.IsValid()) {
        return Shared::Types::Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER, "心跳配置无效"));
    }

    // 如果已经在心跳，先停止
    if (heartbeat_active_) {
        StopHeartbeat();
    }

    // 保存配置
    heartbeat_config_ = config;

    // 重置状态
    heartbeat_status_ = Siligen::Domain::Machine::Ports::HeartbeatStatus{};
    heartbeat_status_.is_active = true;

    should_heartbeat_ = true;
    heartbeat_active_ = true;

    // 启动心跳线程
    try {
        heartbeat_thread_ = std::thread([this]() { HeartbeatLoop(); });
    } catch (const std::exception& e) {
        heartbeat_active_ = false;
        should_heartbeat_ = false;
        return Shared::Types::Result<void>::Failure(Shared::Types::Error(Shared::Types::ErrorCode::THREAD_START_FAILED,
                                                                         std::string("启动心跳线程失败: ") + e.what()));
    }

    return Shared::Types::Result<void>::Success();
}

void HardwareConnectionAdapter::StopHeartbeat() {
    should_heartbeat_ = false;

    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }

    std::lock_guard<std::mutex> lock(heartbeat_mutex_);
    heartbeat_active_ = false;
    heartbeat_status_.is_active = false;
}

Shared::Types::Result<bool> HardwareConnectionAdapter::Ping() const {
    return ExecuteHeartbeat();
}

Siligen::Domain::Machine::Ports::HeartbeatStatus HardwareConnectionAdapter::GetHeartbeatStatus() const {
    std::lock_guard<std::mutex> lock(heartbeat_mutex_);

    // 复制状态并添加配置信息
    Siligen::Domain::Machine::Ports::HeartbeatStatus status = heartbeat_status_;
    status.interval_ms = heartbeat_config_.interval_ms;
    status.failure_threshold = heartbeat_config_.failure_threshold;

    return status;
}

void HardwareConnectionAdapter::HeartbeatLoop() {
    SILIGEN_LOG_INFO("Heartbeat thread started");

    while (should_heartbeat_) {
        auto result = ExecuteHeartbeat();

        // 更新心跳统计
        {
            std::lock_guard<std::mutex> lock(heartbeat_mutex_);
            heartbeat_status_.total_sent++;

            if (result.IsSuccess() && result.Value()) {
                heartbeat_status_.total_success++;
                heartbeat_status_.consecutive_failures = 0;
                heartbeat_status_.last_error.clear();

                // 自动恢复：如果之前因心跳失败断开，现在恢复为已连接
                if (connection_state_ == Siligen::Domain::Machine::Ports::HardwareConnectionState::Disconnected) {
                    SILIGEN_LOG_INFO("Heartbeat recovered! Auto-reconnecting...");

                    std::lock_guard<std::mutex> conn_lock(connection_mutex_);
                    connection_state_ = Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected;
                    last_error_.clear();

                    connection_info_.state = connection_state_;
                    connection_info_.error_message.clear();

                    // 触发回调通知Application层
                    if (state_change_callback_) {
                        try {
                            state_change_callback_(connection_info_);
                        } catch (const std::exception& e) {
                            // 回调异常不应中断心跳循环
                            SILIGEN_LOG_WARNING(std::string("State change callback threw exception: ") + e.what());
                        }
                    }
                }

                // 添加日志：每 10 次成功心跳记录一次
                if (heartbeat_status_.total_success % 10 == 0) {
                    SILIGEN_LOG_INFO("Heartbeat OK (total: " + std::to_string(heartbeat_status_.total_success) + " successes)");
                }
            } else {
                heartbeat_status_.total_failure++;
                heartbeat_status_.consecutive_failures++;
                heartbeat_status_.last_error = result.IsError() ? result.GetError().GetMessage() : "硬件无响应";

                // 仅在首次失败时记录（避免日志刷屏）
                if (heartbeat_status_.consecutive_failures == 1) {
                    SILIGEN_LOG_WARNING("Heartbeat failure detected, waiting for threshold...");
                }
            }
        }

        // 如果达到失败阈值，更新连接状态并触发回调
        // （在心跳锁范围外执行，避免死锁）
        {
            std::lock_guard<std::mutex> lock(heartbeat_mutex_);
            if (heartbeat_status_.consecutive_failures >= heartbeat_config_.failure_threshold) {
                // 退出心跳锁范围，准备获取连接锁
                // （避免在持锁状态下调用回调，防止死锁）
            }
        }

        // 再次检查阈值（可能已被其他线程修改）
        if (heartbeat_status_.consecutive_failures >= heartbeat_config_.failure_threshold) {
            // 修复方案: 进入降级模式而非断开连接
            // 心跳失败不应阻止命令执行，而是降级为"不可靠但可用"状态
            if (!heartbeat_status_.is_degraded) {
                SILIGEN_LOG_WARNING("Entering degraded mode: heartbeat unavailable");
                SILIGEN_LOG_INFO("Connection remains active for command execution");
                SILIGEN_LOG_WARNING("Consecutive failures: " + std::to_string(heartbeat_status_.consecutive_failures));

                // 更新心跳状态为降级模式（但不影响连接状态）
                heartbeat_status_.is_degraded = true;
                heartbeat_status_.degraded_reason = "MC_GetSts unavailable (error -999)";

                // 通知前端进入降级模式（但不断开连接）
                connection_info_.state = connection_state_;  // 保持 Connected
                connection_info_.error_message = "降级模式: 心跳不可用，连接保持活跃";

                auto callback_copy = state_change_callback_;
                if (callback_copy) {
                    try {
                        callback_copy(connection_info_);
                    } catch (const std::exception& e) {
                        SILIGEN_LOG_WARNING(std::string("Callback failed: ") + e.what());
                    }
                }

                // 降低心跳频率（从 500ms 降到 5000ms），减少资源占用
                SILIGEN_LOG_INFO("Reducing heartbeat frequency to 5 seconds");
            }

            // 不 break，继续心跳循环，但降低频率
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            continue;
        }

        // 等待下一次心跳
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_config_.interval_ms));
    }

    SILIGEN_LOG_INFO("Heartbeat thread stopped");
}

Shared::Types::Result<bool> HardwareConnectionAdapter::ExecuteHeartbeat() const {
    // 检查软件层连接状态
    if (connection_state_ != Siligen::Domain::Machine::Ports::HardwareConnectionState::Connected) {
        return Shared::Types::Result<bool>::Success(false);
    }

    // 检查适配器是否存在
    if (!multicard_adapter_) {
        return Shared::Types::Result<bool>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::ADAPTER_NOT_INITIALIZED, "MultiCard适配器未初始化"));
    }

    // 调用 MultiCard 适配器的 IsConnected()
    // 这会触发 MC_GetSts(0) 调用，产生 UDP 心跳请求
    return multicard_adapter_->IsConnected();
}

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen







