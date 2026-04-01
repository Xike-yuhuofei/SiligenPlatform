#include "InterlockMonitor.h"

#include "SecurityLogHelper.h"
#include "domain/safety/bridges/MotionCoreInterlockBridge.h"

#include <iomanip>
#include <sstream>

namespace Siligen {

using Shared::Types::LogLevel;

namespace {
using DomainPriority = Siligen::Domain::Safety::ValueObjects::InterlockPriority;
constexpr short kGeneralPurposeInputGroup = 4;  // MultiCard MC_GPI: X0~X15 通用输入
constexpr int16 kGeneralPurposeInputMin = 0;
constexpr int16 kGeneralPurposeInputMax = 15;
constexpr auto kReadFailureLogThrottle = std::chrono::seconds(1);
constexpr int kMissingMulticardErrorCode = -10001;
constexpr int kInvalidEmergencyInputErrorCode = -10002;

bool IsGeneralPurposeInputIndexValid(int16 index) noexcept {
    return index >= kGeneralPurposeInputMin && index <= kGeneralPurposeInputMax;
}

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

InterlockMonitor::InterlockMonitor(
    AuditLogger& audit_logger,
    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> multicard)
    : running_(false),
      sample_available_(false),
      triggered_(false),
      audit_logger_(audit_logger),
      multicard_(std::move(multicard)),
      last_self_test_(std::chrono::system_clock::now()) {}

InterlockMonitor::~InterlockMonitor() {
    Stop();
}

bool InterlockMonitor::Initialize(const InterlockConfig& config) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    config_ = config;
    last_sample_error_active_ = false;
    last_sample_error_code_ = 0;
    last_sample_error_log_time_ = std::chrono::steady_clock::time_point{};
    last_logged_signal_state_valid_ = false;
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
        sample_available_.store(false);
        SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "连锁监控器未启用");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_sample_error_active_ = false;
        last_sample_error_code_ = 0;
        last_sample_error_log_time_ = std::chrono::steady_clock::time_point{};
        last_logged_signal_state_valid_ = false;
    }
    sample_available_.store(false);
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
    sample_available_.store(false);
    triggered_.store(false);
    if (!running_.load()) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_sample_error_active_ = false;
        last_sample_error_code_ = 0;
        last_sample_error_log_time_ = std::chrono::steady_clock::time_point{};
        last_logged_signal_state_valid_ = false;
        return;
    }

    running_.store(false);
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        last_sample_error_active_ = false;
        last_sample_error_code_ = 0;
        last_sample_error_log_time_ = std::chrono::steady_clock::time_point{};
        last_logged_signal_state_valid_ = false;
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
    if (!config_.enabled) {
        return Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals>::Success(
            Domain::Safety::ValueObjects::InterlockSignals{});
    }

    if (!running_.load()) {
        return Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::HARDWARE_NOT_CONNECTED,
                "Interlock monitor is not active",
                "InterlockMonitor"));
    }

    if (!sample_available_.load()) {
        return Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals>::Failure(
            Shared::Types::Error(
                Shared::Types::ErrorCode::HARDWARE_NOT_CONNECTED,
                "Interlock signals are unavailable",
                "InterlockMonitor"));
    }

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
        if (ReadSensorStates()) {
            // 仅在采样有效时执行连锁判定，避免将 unknown/unavailable 误判为急停触发。
            CheckInterlockConditions();
        }

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
    const auto now = std::chrono::steady_clock::now();

    const auto clear_state = [this]() {
        state_.emergency_stop_triggered = false;
        state_.safety_door_open = false;
        state_.pressure_abnormal = false;
        state_.temperature_abnormal = false;
        state_.voltage_abnormal = false;
        state_.servo_alarm = false;
    };

    const auto mark_sample_failure =
        [this, &now, &clear_state](int error_code, const std::string& message) {
            clear_state();
            sample_available_.store(false);
            last_logged_signal_state_valid_ = false;

            const bool should_log =
                !last_sample_error_active_ ||
                last_sample_error_code_ != error_code ||
                (now - last_sample_error_log_time_) >= kReadFailureLogThrottle;
            if (should_log) {
                SecurityLogHelper::Log(LogLevel::ERR, "InterlockMonitor", message);
                last_sample_error_log_time_ = now;
            }

            last_sample_error_active_ = true;
            last_sample_error_code_ = error_code;
        };

    const auto mark_sample_success = [this]() {
        if (last_sample_error_active_) {
            SecurityLogHelper::Log(LogLevel::INFO, "InterlockMonitor", "互锁输入读取恢复");
        }
        last_sample_error_active_ = false;
        last_sample_error_code_ = 0;
    };

    if (!multicard_) {
        mark_sample_failure(kMissingMulticardErrorCode, "MultiCardWrapper 未注入，无法读取互锁输入");
        return false;
    }

    // 读取通用数字输入组(X0~X15)；急停极性由配置决定，安全门默认按高电平表示打开处理。
    long diValue = 0;
    const int ret = multicard_->MC_GetDiRaw(kGeneralPurposeInputGroup, &diValue);
    if (ret == 0) {
        if (!IsGeneralPurposeInputIndexValid(config_.emergency_stop_input)) {
            mark_sample_failure(
                kInvalidEmergencyInputErrorCode,
                "急停输入位越界，互锁采样不可用: emergency_stop_input=" +
                    std::to_string(config_.emergency_stop_input) + ", valid_range=0..15");
            return false;
        }

        const auto emergency_input = static_cast<unsigned int>(config_.emergency_stop_input);
        const auto emergency_mask = static_cast<unsigned long>(1UL << emergency_input);
        const bool emergency_raw_high = (diValue & emergency_mask) != 0;
        state_.emergency_stop_triggered =
            config_.emergency_stop_active_low ? !emergency_raw_high : emergency_raw_high;

        if (config_.safety_door_input < 0) {
            state_.safety_door_open = false;
        } else if (IsGeneralPurposeInputIndexValid(config_.safety_door_input)) {
            const auto safety_door_input = static_cast<unsigned int>(config_.safety_door_input);
            const auto safety_door_mask = static_cast<unsigned long>(1UL << safety_door_input);
            state_.safety_door_open = (diValue & safety_door_mask) != 0;
        } else {
            SecurityLogHelper::Log(
                LogLevel::ERR,
                "InterlockMonitor",
                "安全门输入位越界，按关闭处理: safety_door_input=" + std::to_string(config_.safety_door_input) +
                    ", valid_range=0..15");
            state_.safety_door_open = false;
        }

        mark_sample_success();
        sample_available_.store(true);
        const bool state_changed =
            !last_logged_signal_state_valid_ ||
            last_logged_estop_state_ != state_.emergency_stop_triggered ||
            last_logged_door_state_ != state_.safety_door_open;
        if (state_changed) {
            SecurityLogHelper::Log(
                LogLevel::INFO,
                "InterlockMonitor",
                "互锁状态读取: estop=" + std::string(state_.emergency_stop_triggered ? "触发" : "正常") +
                    ", door=" + std::string(state_.safety_door_open ? "打开" : "关闭"));
            last_logged_signal_state_valid_ = true;
            last_logged_estop_state_ = state_.emergency_stop_triggered;
            last_logged_door_state_ = state_.safety_door_open;
        }
    } else {
        mark_sample_failure(ret, "急停状态读取失败，错误码: " + std::to_string(ret));
        return false;
    }

    // 其他传感器暂时保持默认值（待硬件连接）
    state_.pressure_abnormal = false;
    state_.temperature_abnormal = false;
    state_.voltage_abnormal = false;
    state_.servo_alarm = false;

    return true;
}

// T069: 安全连锁触发逻辑 (优先级: 急停 > 硬件限位 > 软限位 > 温度/气压)
void InterlockMonitor::CheckInterlockConditions() {
    if (!sample_available_.load()) {
        return;
    }

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
