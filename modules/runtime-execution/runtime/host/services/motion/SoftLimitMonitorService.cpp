#include "SoftLimitMonitorService.h"

#include "domain/safety/domain-services/InterlockPolicy.h"
#include "shared/types/Error.h"

#include <chrono>
#include <sstream>
#include <stdexcept>

namespace Siligen::Application::Services::Motion {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToUserDisplay;
using Siligen::Shared::Types::int16;

SoftLimitMonitorService::SoftLimitMonitorService(
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
    const SoftLimitMonitorConfig& config)
    : motion_state_port_(std::move(motion_state_port))
    , event_port_(std::move(event_port))
    , position_control_port_(std::move(position_control_port))
    , config_(config) {
    if (!motion_state_port_ || !event_port_ || !position_control_port_) {
        throw std::invalid_argument("Motion state port, event port, and position control port cannot be null");
    }
}

SoftLimitMonitorService::~SoftLimitMonitorService() {
    Stop();
}

Result<void> SoftLimitMonitorService::Start() noexcept {
    // 防止重复启动
    if (is_running_.load()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Soft limit monitor is already running"));
    }

    // 检查配置是否启用
    if (!config_.enabled) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Soft limit monitor is disabled in configuration"));
    }

    // 启动监控线程
    stop_requested_.store(false);
    is_running_.store(true);

    try {
        monitoring_thread_ = std::thread([this]() {
            this->MonitoringLoop();
        });
    } catch (const std::exception& e) {
        is_running_.store(false);
        return Result<void>::Failure(Error(
            ErrorCode::UNKNOWN_ERROR,
            std::string("Failed to create monitoring thread: ") + e.what()));
    }

    return Result<void>::Success();
}

Result<void> SoftLimitMonitorService::Stop() noexcept {
    // 如果未运行，直接返回成功
    if (!is_running_.load()) {
        return Result<void>::Success();
    }

    // 请求停止
    stop_requested_.store(true);

    // 等待线程结束
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    is_running_.store(false);

    return Result<void>::Success();
}

Result<bool> SoftLimitMonitorService::CheckSoftLimits() noexcept {
    // 遍历所有监控的轴
    for (auto axis_id : config_.monitored_axes) {
        if (!IsValid(axis_id)) {
            continue;
        }
        auto status_result = motion_state_port_->GetAxisStatus(axis_id);
        if (!status_result.IsSuccess()) {
            // 无法读取轴状态，跳过
            continue;
        }

        const auto& status = status_result.Value();

        bool positive_limit = false;
        const bool triggered = Siligen::Domain::Safety::DomainServices::InterlockPolicy::IsSoftLimitTriggered(
            status, &positive_limit, nullptr);
        if (triggered) {
            auto handle_result = HandleSoftLimitTrigger(axis_id, status, positive_limit);
            if (!handle_result.IsSuccess()) {
                // 处理失败，但继续检查
            }
            return Result<bool>::Success(true);
        }
    }

    return Result<bool>::Success(false);
}

void SoftLimitMonitorService::MonitoringLoop() noexcept {
    while (!stop_requested_.load()) {
        // 执行软限位检查
        auto check_result = CheckSoftLimits();

        if (check_result.IsSuccess() && check_result.Value()) {
            // 检测到软限位触发
            if (config_.auto_stop_on_trigger) {
                // 继续监控以防止后续触发
                // 注意: 这里不停止监控服务，继续运行
            }
        }

        // 等待下一次轮询
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.monitoring_interval_ms));
    }
}

Result<void> SoftLimitMonitorService::HandleSoftLimitTrigger(
    LogicalAxisId axis_id,
    const Domain::Motion::Ports::MotionStatus& status,
    bool positive_limit) noexcept {

    // 确定是正向还是负向限位
    const int16 axis_display = ToUserDisplay(axis_id);

    // 创建软限位触发事件
    Domain::System::Ports::SoftLimitTriggeredEvent event;
    event.type = Domain::System::Ports::EventType::SOFT_LIMIT_TRIGGERED;
    event.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    event.source = "SoftLimitMonitorService";
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        event.task_id = current_task_id_;
    }
    event.axis = axis_id;
    event.position = status.axis_position_mm;
    event.is_positive_limit = positive_limit;

    // 构建事件消息
    std::ostringstream oss;
    oss << "Soft limit "
        << (positive_limit ? "positive" : "negative")
        << " triggered on axis " << axis_display
        << " at axis position " << event.position << "mm";
    event.message = oss.str();

    // 发布事件
    auto publish_result = event_port_->Publish(event);
    if (!publish_result.IsSuccess()) {
        // 记录错误但不中断处理
        // TODO: 添加日志记录
    }

    // 调用用户回调（线程安全）
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (trigger_callback_) {
            trigger_callback_(axis_id, event.position, positive_limit);
        }
    }

    // 根据配置自动停止运动
    if (config_.auto_stop_on_trigger) {
        auto stop_result = StopMotion(config_.emergency_stop_on_trigger);
        if (!stop_result.IsSuccess()) {
            // 记录错误
            // TODO: 添加日志记录
        }
    }

    return Result<void>::Success();
}

Result<void> SoftLimitMonitorService::StopMotion(bool use_emergency_stop) noexcept {
    if (use_emergency_stop) {
        if (!position_control_port_) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_STATE,
                "Position control port not available",
                "SoftLimitMonitorService::StopMotion"));
        }
        return position_control_port_->EmergencyStop();
    }

    if (!position_control_port_) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Position control port not available",
            "SoftLimitMonitorService::StopMotion"));
    }

    return position_control_port_->StopAllAxes(false);
}

}  // namespace Siligen::Application::Services::Motion
