#pragma once

#include "runtime_execution/application/usecases/system/IHardLimitMonitor.h"
#include "runtime_execution/contracts/motion/IIOControlPort.h"
#include "runtime_execution/contracts/motion/IPositionControlPort.h"
#include "shared/types/Result.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace Siligen::Application::Services::Motion {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::uint32;

/**
 * @brief 硬限位监控配置
 */
struct HardLimitMonitorConfig {
    bool enabled = true;                         ///< 是否启用监控
    uint32 monitoring_interval_ms = 5;           ///< 监控轮询间隔（毫秒）
    std::vector<LogicalAxisId> monitored_axes = {LogicalAxisId::X, LogicalAxisId::Y};  ///< 逻辑轴号列表
    bool emergency_stop_on_trigger = true;       ///< 触发时是否立即停止该轴
};

/**
 * @brief 硬限位监控服务（应用层）
 *
 * 后台轮询限位状态，检测触发并停止触发轴。
 * 仅依赖端口接口，不直接访问硬件。
 * 互锁触发判定必须委托 `Domain::Safety::DomainServices::InterlockPolicy`。
 */
class HardLimitMonitorService : public UseCases::System::IHardLimitMonitor {
   public:
    explicit HardLimitMonitorService(
        std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port,
        std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
        const HardLimitMonitorConfig& config = HardLimitMonitorConfig{});

    ~HardLimitMonitorService();

    HardLimitMonitorService(const HardLimitMonitorService&) = delete;
    HardLimitMonitorService& operator=(const HardLimitMonitorService&) = delete;
    HardLimitMonitorService(HardLimitMonitorService&&) = delete;
    HardLimitMonitorService& operator=(HardLimitMonitorService&&) = delete;

    Result<void> Start() noexcept override;
    Result<void> Stop() noexcept override;

    bool IsRunning() const noexcept override {
        return is_running_.load();
    }

   private:
    void MonitoringLoop() noexcept;

    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port_;
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;
    HardLimitMonitorConfig config_;

    std::thread monitoring_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_requested_{false};

    std::vector<bool> last_triggered_;
};

}  // namespace Siligen::Application::Services::Motion
