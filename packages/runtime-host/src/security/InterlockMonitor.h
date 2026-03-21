#pragma once

#include "shared/types/Types.h"
#include "security/AuditLogger.h"
#include "domain/safety/ports/IInterlockSignalPort.h"
#include "siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace Siligen {

// 传感器类型枚举
enum class SensorType : int8 {
    EMERGENCY_STOP = 0,
    SAFETY_DOOR = 1,
    PRESSURE = 2,
    TEMPERATURE = 3,
    VOLTAGE = 4,
    SERVO_ALARM = 5
};

// 连锁优先级 (值越小优先级越高)
enum class InterlockPriority : int8 {
    CRITICAL = 0,  // 急停
    HIGH = 1,      // 硬件限位
    MEDIUM = 2,    // 软限位
    LOW = 3        // 温度/气压/电压
};

// 连锁状态
struct InterlockState {
    bool emergency_stop_triggered;
    bool safety_door_open;
    bool pressure_abnormal;
    bool temperature_abnormal;
    bool voltage_abnormal;
    bool servo_alarm;
    std::chrono::system_clock::time_point last_update;

    InterlockState()
        : emergency_stop_triggered(false),
          safety_door_open(false),
          pressure_abnormal(false),
          temperature_abnormal(false),
          voltage_abnormal(false),
          servo_alarm(false),
          last_update(std::chrono::system_clock::now()) {}
};

// 连锁配置
struct InterlockConfig {
    bool enabled;
    int16 emergency_stop_input;
    bool emergency_stop_active_low;
    int16 safety_door_input;
    int16 pressure_sensor_input;
    int16 temperature_sensor_input;
    int16 voltage_sensor_input;
    int32 poll_interval_ms;
    float32 pressure_critical_low;
    float32 pressure_warning_low;
    float32 pressure_warning_high;
    float32 pressure_critical_high;
    float32 temperature_critical_low;
    float32 temperature_normal_low;
    float32 temperature_normal_high;
    float32 temperature_critical_high;
    float32 voltage_min;
    float32 voltage_max;
    int32 self_test_interval_hours;
};

// 互锁信号采集器：仅采集/转发信号，判定规则委托 Domain::Safety::InterlockPolicy
class InterlockMonitor : public Domain::Safety::Ports::IInterlockSignalPort {
   public:
    using InterlockCallback = std::function<void(const std::string&)>;

    explicit InterlockMonitor(
        AuditLogger& audit_logger,
        std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> multicard = nullptr);
    ~InterlockMonitor();

    // 禁止拷贝
    InterlockMonitor(const InterlockMonitor&) = delete;
    InterlockMonitor& operator=(const InterlockMonitor&) = delete;

    bool Initialize(const InterlockConfig& config);
    void Start();
    void Stop();
    InterlockState GetState() const;
    bool IsTriggered() const;
    void SetCallback(InterlockCallback callback);
    Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals> ReadSignals() const noexcept override;

   private:
    void MonitoringLoop();
    bool ReadSensorStates();
    void CheckInterlockConditions();
    void TriggerInterlock(const std::string& reason, InterlockPriority priority);
    void PerformSelfTest();

    InterlockConfig config_;
    InterlockState state_;
    mutable std::mutex state_mutex_;
    std::atomic<bool> running_;
    std::atomic<bool> triggered_;
    std::thread monitor_thread_;
    InterlockCallback callback_;
    AuditLogger& audit_logger_;
    std::shared_ptr<Infrastructure::Hardware::IMultiCardWrapper> multicard_;
    std::chrono::system_clock::time_point last_self_test_;
    Domain::Safety::ValueObjects::InterlockPolicyConfig policy_config_;
};

}  // namespace Siligen

