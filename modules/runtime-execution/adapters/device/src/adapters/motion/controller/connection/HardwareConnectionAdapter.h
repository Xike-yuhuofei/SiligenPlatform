#pragma once

#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Types.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace Siligen {
namespace Infrastructure {
namespace Hardware {
class MultiCardAdapter;
}  // namespace Hardware

namespace Adapters {

/**
 * @brief 硬件连接适配器
 *
 * 实现IHardwareConnectionPort接口，封装MultiCard硬件连接逻辑
 * 作为基础设施层的适配器，负责将领域接口转换为具体的硬件操作
 */
class HardwareConnectionAdapter : public Siligen::Domain::Machine::Ports::IHardwareConnectionPort {
   public:
    /**
     * @brief 构造函数
     * @param multicard 可选的共享IMultiCardWrapper实例
     * @param mode 硬件模式（Real=真实硬件, Mock=测试Mock）
     */
    explicit HardwareConnectionAdapter(
        std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> multicard,
        Siligen::Shared::Types::HardwareMode mode,
        const Siligen::Shared::Types::HardwareConfiguration& hardware_config);

    /**
     * @brief 析构函数
     */
    ~HardwareConnectionAdapter() override;

    // 禁止拷贝和移动
    HardwareConnectionAdapter(const HardwareConnectionAdapter&) = delete;
    HardwareConnectionAdapter& operator=(const HardwareConnectionAdapter&) = delete;
    HardwareConnectionAdapter(HardwareConnectionAdapter&&) = delete;
    HardwareConnectionAdapter& operator=(HardwareConnectionAdapter&&) = delete;

    // IHardwareConnectionPort 接口实现
    Shared::Types::Result<void> Connect(const Siligen::Domain::Machine::Ports::HardwareConnectionConfig& config) override;
    Shared::Types::Result<void> Disconnect() override;
    Siligen::Domain::Machine::Ports::HardwareConnectionInfo GetConnectionInfo() const override;
    bool IsConnected() const override;
    Shared::Types::Result<void> Reconnect() override;

    void SetConnectionStateCallback(
        std::function<void(const Siligen::Domain::Machine::Ports::HardwareConnectionInfo&)> callback) override;

    Shared::Types::Result<void> StartStatusMonitoring(uint32 interval_ms = 1000) override;
    void StopStatusMonitoring() override;

    std::string GetLastError() const override;
    void ClearError() override;

    // IHardwareConnectionPort 心跳接口实现
    Shared::Types::Result<void> StartHeartbeat(const Siligen::Domain::Machine::Ports::HeartbeatConfig& config) override;
    void StopHeartbeat() override;
    Siligen::Domain::Machine::Ports::HeartbeatStatus GetHeartbeatStatus() const override;
    Shared::Types::Result<bool> Ping() const override;

   private:
    /**
     * @brief 监控循环
     * @param interval_ms 监控间隔
     */
    void MonitoringLoop(uint32 interval_ms);

    /**
     * @brief 更新设备信息
     */
    void UpdateDeviceInfo();

    /**
     * @brief 心跳循环
     */
    void HeartbeatLoop();

    /**
     * @brief 执行单次心跳检测
     * @return 检测结果
     */
    Shared::Types::Result<bool> ExecuteHeartbeat() const;

   private:
    // 连接状态
    mutable std::mutex connection_mutex_;                      ///< 连接状态互斥锁
    Siligen::Domain::Machine::Ports::HardwareConnectionState connection_state_;  ///< 连接状态
    std::string last_error_;                                   ///< 最后错误信息
    Siligen::Domain::Machine::Ports::HardwareConnectionInfo connection_info_;    ///< 连接信息
    std::chrono::steady_clock::time_point last_connect_time_{};  ///< 上次成功连接时间（用于初始化宽限期）

    // 硬件相关
    void* hardware_handle_;                                                                   ///< 硬件句柄
    std::unique_ptr<Siligen::Infrastructure::Hardware::MultiCardAdapter> multicard_adapter_;  ///< MultiCard适配器
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper>
        shared_multicard_;                                ///< 共享的IMultiCardWrapper实例
    Siligen::Shared::Types::HardwareMode hardware_mode_;  ///< 硬件模式标志
    Siligen::Shared::Types::HardwareConfiguration hardware_config_;  ///< 硬件配置
    // 监控相关
    std::thread monitoring_thread_;               ///< 监控线程
    std::atomic<bool> should_monitor_{false};     ///< 是否应该继续监控
    std::atomic<bool> monitoring_active_{false};  ///< 监控是否活跃

    // 心跳相关
    std::thread heartbeat_thread_;                     ///< 心跳线程
    std::atomic<bool> should_heartbeat_{false};        ///< 是否应该继续心跳
    std::atomic<bool> heartbeat_active_{false};        ///< 心跳是否活跃
    mutable std::mutex heartbeat_mutex_;               ///< 心跳状态互斥锁 (必须在config和status之前声明)
    Siligen::Domain::Machine::Ports::HeartbeatConfig heartbeat_config_;  ///< 心跳配置
    Siligen::Domain::Machine::Ports::HeartbeatStatus heartbeat_status_;  ///< 心跳状态

    // 回调函数
    std::function<void(const Siligen::Domain::Machine::Ports::HardwareConnectionInfo&)> state_change_callback_;  ///< 状态变化回调
};

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen









