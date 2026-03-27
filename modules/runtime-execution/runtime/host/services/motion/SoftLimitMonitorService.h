#pragma once

#include "runtime_execution/contracts/system/IEventPublisherPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "shared/types/Result.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Siligen::Domain::Motion::DomainServices {
}

namespace Siligen::Application::Services::Motion {

using Shared::Types::Result;
using Shared::Types::LogicalAxisId;
using Shared::Types::float32;
using Shared::Types::uint32;
using RuntimeEventPublisherPort = Siligen::RuntimeExecution::Contracts::System::IEventPublisherPort;

/**
 * @brief 软限位监控配置
 */
struct SoftLimitMonitorConfig {
    bool enabled = true;                        ///< 是否启用监控
    uint32 monitoring_interval_ms = 100;        ///< 监控轮询间隔（毫秒）
    std::vector<LogicalAxisId> monitored_axes = {LogicalAxisId::X, LogicalAxisId::Y};
    bool auto_stop_on_trigger = true;           ///< 检测到触发时自动停止
    bool emergency_stop_on_trigger = false;     ///< 是否使用急停（而非普通停止）
};

/**
 * @brief 软限位监控回调
 *
 * 用户可设置回调函数自定义软限位触发时的处理逻辑
 */
using SoftLimitTriggerCallback = std::function<void(
    LogicalAxisId axis,       ///< 触发轴号
    float32 position, ///< 当前位置
    bool positive_limit ///< 正向或负向限位
)>;

/**
 * @brief 软限位监控服务
 *
 * 后台监控轴状态，检测软限位触发并发布事件。
 * 符合应用层约束: 依赖端口接口，不直接访问硬件。
 * 互锁触发判定必须委托 `Domain::Safety::DomainServices::InterlockPolicy`。
 *
 * ## 设计要点
 * - 后台线程轮询轴状态（100ms间隔）
 * - 检测到触发时发布事件并调用回调
 * - 可配置自动停止/急停
 * - 线程安全：独立mutex保护共享数据
 * - RAII：析构时自动停止监控
 */
class SoftLimitMonitorService {
   public:
    /**
     * @brief 构造函数
     *
     * @param motion_state_port 运动状态端口
     * @param event_port 事件发布端口
     * @param config 监控配置
     */
    explicit SoftLimitMonitorService(
        std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<RuntimeEventPublisherPort> event_port,
        std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
        const SoftLimitMonitorConfig& config = SoftLimitMonitorConfig{});

    /**
     * @brief 析构函数 - 自动停止监控
     */
    ~SoftLimitMonitorService();

    // 禁止拷贝和移动
    SoftLimitMonitorService(const SoftLimitMonitorService&) = delete;
    SoftLimitMonitorService& operator=(const SoftLimitMonitorService&) = delete;

    /**
     * @brief 启动监控
     *
     * @return Result<void> 成功返回Success，失败返回错误信息
     */
    Result<void> Start() noexcept;

    /**
     * @brief 停止监控
     *
     * @return Result<void> 成功返回Success
     */
    Result<void> Stop() noexcept;

    /**
     * @brief 检查监控是否运行中
     */
    bool IsRunning() const noexcept {
        return is_running_.load();
    }

    /**
     * @brief 设置软限位触发回调
     *
     * @param callback 回调函数
     */
    void SetTriggerCallback(SoftLimitTriggerCallback callback) noexcept {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        trigger_callback_ = std::move(callback);
    }

    /**
     * @brief 手动检查软限位状态（单次）
     *
     * @return Result<bool> true=检测到触发, false=未触发
     */
    Result<bool> CheckSoftLimits() noexcept;

    /**
     * @brief 关联当前任务ID（用于事件追踪）
     *
     * @param task_id 任务ID
     */
    void SetCurrentTaskId(const std::string& task_id) noexcept {
        std::lock_guard<std::mutex> lock(task_mutex_);
        current_task_id_ = task_id;
    }

    /**
     * @brief 清除当前任务ID
     */
    void ClearCurrentTaskId() noexcept {
        std::lock_guard<std::mutex> lock(task_mutex_);
        current_task_id_.clear();
    }

   private:
    /**
     * @brief 监控循环（后台线程执行）
     */
    void MonitoringLoop() noexcept;

    /**
     * @brief 处理软限位触发
     *
     * @param axis 轴号
     * @param status 轴状态
     * @return Result<void>
     */
    Result<void> HandleSoftLimitTrigger(
        LogicalAxisId axis,
        const Domain::Motion::Ports::MotionStatus& status,
        bool positive_limit) noexcept;

    /**
     * @brief 执行停止运动
     *
     * @param use_emergency_stop 是否使用急停
     * @return Result<void>
     *
     * @note TODO: 实现停止运动逻辑（需要IMotionControlPort依赖）
     */
    Result<void> StopMotion(bool use_emergency_stop) noexcept;

    // 端口依赖
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port_;
    std::shared_ptr<RuntimeEventPublisherPort> event_port_;
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port_;

    // 配置
    SoftLimitMonitorConfig config_;

    // 监控线程
    std::thread monitoring_thread_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_requested_{false};

    // 回调函数
    SoftLimitTriggerCallback trigger_callback_;
    std::mutex callback_mutex_;

    // 任务追踪
    std::string current_task_id_;
    std::mutex task_mutex_;
};

}  // namespace Siligen::Application::Services::Motion
