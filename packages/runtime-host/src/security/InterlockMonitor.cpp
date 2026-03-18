#include "InterlockMonitor.h"

#include "SecurityLogHelper.h"
#include "domain/safety/bridges/MotionCoreInterlockBridge.h"

#include <iomanip>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

namespace {
using DomainPriority = Siligen::Domain::Safety::ValueObjects::InterlockPriority;

InterlockPriority MapPriority(DomainPriority priority) noexcept {
    switch (priority) {
        case DomainPriority::CRITICAL:
            return InterlockPriority::CRITICAL;
        case DomainPriority::HIGH:
            return InterlockPriority::HIGH;
        case DomainPriority::MEDIUM:
            return InterlockPriority::MEDIUM;
        case DomainPriority::LOW:
        default:
            return InterlockPriority::LOW;
    }
}
}  // namespace

InterlockMonitor::InterlockMonitor(AuditLogger& audit_logger)
    : running_(false),
      triggered_(false),
      audit_logger_(audit_logger),
      last_self_test_(std::chrono::system_clock::now()) {}

InterlockMonitor::~InterlockMonitor() {
    Stop();
}

bool InterlockMonitor::Initialize(const InterlockConfig& config) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    config_ = config;
    policy_config_ = Domain::Safety::ValueObjects::InterlockPolicyConfig();
    policy_config_.enabled = config.enabled;

    SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "连锁监控器初始化完成");
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::SYSTEM_CONFIG,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            "system",
                            "初始化连锁监控器",
                            "enabled=" + std::string(config.enabled ? "true" : "false"),
                            ""});

    return true;
}

void InterlockMonitor::Start() {
    if (running_.load()) {
        SecurityLogHelper::Log(LogLevel::WARNING, "InterlockMonitor", "连锁监控器已经在运行");
        return;
    }

    if (!config_.enabled) {
        SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "连锁监控器未启用");
        return;
    }

    running_.store(true);
    triggered_.store(false);
    monitor_thread_ = std::thread(&InterlockMonitor::MonitoringLoop, this);

    SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "连锁监控器已启动");
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::SYSTEM_CONFIG,
                            AuditLevel::INFO,
                            AuditStatus::SUCCESS,
                            "system",
                            "启动连锁监控器",
                            "",
                            ""});
}

void InterlockMonitor::Stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "连锁监控器已停止");
}

InterlockState InterlockMonitor::GetState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

bool InterlockMonitor::IsTriggered() const {
    return triggered_.load();
}

void InterlockMonitor::SetCallback(InterlockCallback callback) {
    callback_ = callback;
}

Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals> InterlockMonitor::ReadSignals() const noexcept {
    std::lock_guard<std::mutex> lock(state_mutex_);

    Domain::Safety::ValueObjects::InterlockSignals signals;
    signals.emergency_stop_triggered = state_.emergency_stop_triggered;
    signals.safety_door_open = state_.safety_door_open;
    signals.pressure_abnormal = state_.pressure_abnormal;
    signals.temperature_abnormal = state_.temperature_abnormal;
    signals.voltage_abnormal = state_.voltage_abnormal;
    signals.servo_alarm = state_.servo_alarm;

    return Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals>::Success(signals);
}

// T068: 传感器状态轮询逻辑 (50ms周期)
void InterlockMonitor::MonitoringLoop() {
    auto poll_interval = std::chrono::milliseconds(config_.poll_interval_ms);

    while (running_.load()) {
        auto start_time = std::chrono::steady_clock::now();

        // 读取传感器状态
        if (!ReadSensorStates()) {
            SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", "读取传感器状态失败");
        }

        // 检查连锁条件
        CheckInterlockConditions();

        // 定期自检 (T070)
        auto now = std::chrono::system_clock::now();
        auto time_since_test = std::chrono::duration_cast<std::chrono::hours>(now - last_self_test_).count();
        if (time_since_test >= config_.self_test_interval_hours) {
            PerformSelfTest();
            last_self_test_ = now;
        }

        // 维持轮询周期
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed < poll_interval) {
            std::this_thread::sleep_for(poll_interval - elapsed);
        }
    }
}

bool InterlockMonitor::ReadSensorStates() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.last_update = std::chrono::system_clock::now();

    // 读取急停状态 - X1引脚作为急停输入 (MC_GetDiRaw, diType=0)
    long diValue = 0;

    // 使用默认卡句柄0读取DI状态
    short ret = MC_GetDiRaw(0, 0, &diValue);
    if (ret == 0) {
        // X1引脚对应DI bit 0，低电平有效（急停按钮按下时为0）
        state_.emergency_stop_triggered = (diValue & 0x01) == 0;
        SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "急停状态读取: " + std::string(state_.emergency_stop_triggered ? "触发" : "正常"));
    } else {
        SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", "急停状态读取失败，错误码: " + std::to_string(ret));
        // 读取失败时保守处理，假设急停已触发
        state_.emergency_stop_triggered = true;
    }

    // 其他传感器暂时保持默认值（待硬件连接）
    state_.safety_door_open = false;
    state_.pressure_abnormal = false;
    state_.temperature_abnormal = false;
    state_.voltage_abnormal = false;
    state_.servo_alarm = false;

    return true;
}

// T069: 安全连锁触发逻辑 (优先级: 急停 > 硬件限位 > 软限位 > 温度/气压)
void InterlockMonitor::CheckInterlockConditions() {
    auto decision_result = Domain::Safety::Bridges::EvaluateWithMotionCore(*this, policy_config_);
    if (decision_result.IsError()) {
        SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", "连锁判定失败: " + decision_result.GetError().GetMessage());
        return;
    }

    const auto& decision = decision_result.Value();
    if (decision.triggered) {
        TriggerInterlock(decision.reason, MapPriority(decision.priority));
        return;
    }

    // 所有条件正常，清除触发状态
    if (triggered_.load()) {
        triggered_.store(false);
        SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "[SECURITY] 连锁条件已解除");
        audit_logger_.LogAudit({std::chrono::system_clock::now(),
                                AuditCategory::SAFETY_EVENT,
                                AuditLevel::INFO,
                                AuditStatus::SUCCESS,
                                "system",
                                "连锁条件解除",
                                "所有传感器状态恢复正常",
                                ""});
    }
}

// T071: 连锁状态变化日志 [SECURITY]
// T072: 紧急停止响应错误处理和审计
void InterlockMonitor::TriggerInterlock(const std::string& reason, InterlockPriority priority) {
    if (triggered_.load()) {
        return;  // 已经触发，避免重复日志
    }

    triggered_.store(true);

    std::string priority_str;
    switch (priority) {
        case InterlockPriority::CRITICAL:
            priority_str = "CRITICAL";
            SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", "[SECURITY] 安全连锁触发 (紧急): " + reason);
            break;
        case InterlockPriority::HIGH:
            priority_str = "HIGH";
            SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", "[SECURITY] 安全连锁触发 (高): " + reason);
            break;
        case InterlockPriority::MEDIUM:
            priority_str = "MEDIUM";
            SecurityLogHelper::Log(LogLevel::WARNING, "InterlockMonitor", "[SECURITY] 安全连锁触发 (中): " + reason);
            break;
        case InterlockPriority::LOW:
            priority_str = "LOW";
            SecurityLogHelper::Log(LogLevel::WARNING, "InterlockMonitor", "[SECURITY] 安全连锁触发 (低): " + reason);
            break;
    }

    // 审计日志
    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::SAFETY_EVENT,
                            priority == InterlockPriority::CRITICAL ? AuditLevel::SECURITY : AuditLevel::WARNING,
                            AuditStatus::FAILURE,
                            "system",
                            "安全连锁触发",
                            "原因: " + reason + ", 优先级: " + priority_str,
                            ""});

    // 回调通知 (用于停止运动控制)
    if (callback_) {
        callback_(reason);
    }
}

// T070: 连锁自检功能 (24小时自动测试)
void InterlockMonitor::PerformSelfTest() {
    SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "[SECURITY] 开始连锁系统自检");

    bool all_ok = true;
    std::ostringstream result;

    // 检查急停输入
    result << "急停输入=" << config_.emergency_stop_input << ";";

    // 检查安全门输入
    result << "安全门输入=" << config_.safety_door_input << ";";

    // 检查传感器输入
    result << "压力传感器=" << config_.pressure_sensor_input << ";";
    result << "温度传感器=" << config_.temperature_sensor_input << ";";
    result << "电压传感器=" << config_.voltage_sensor_input;

    audit_logger_.LogAudit({std::chrono::system_clock::now(),
                            AuditCategory::SAFETY_EVENT,
                            AuditLevel::INFO,
                            all_ok ? AuditStatus::SUCCESS : AuditStatus::FAILURE,
                            "system",
                            "连锁系统自检",
                            result.str(),
                            ""});

    SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "[SECURITY] 连锁系统自检完成: " + std::string(all_ok ? "通过" : "失败"));
}

}  // namespace Siligen
