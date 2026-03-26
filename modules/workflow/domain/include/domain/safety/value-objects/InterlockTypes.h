#pragma once

#include "shared/types/Types.h"

namespace Siligen {
namespace Domain {
namespace Safety {
namespace ValueObjects {

using Shared::Types::int8;

// 互锁优先级 (值越小优先级越高)
enum class InterlockPriority : int8 {
    CRITICAL = 0,  // 急停
    HIGH = 1,      // 硬件限位/安全门/伺服报警
    MEDIUM = 2,    // 软限位/策略性停机
    LOW = 3        // 温度/气压/电压
};

// 互锁触发原因
enum class InterlockCause : int8 {
    NONE = 0,
    EMERGENCY_STOP = 1,
    SERVO_ALARM = 2,
    SAFETY_DOOR_OPEN = 3,
    PRESSURE_ABNORMAL = 4,
    TEMPERATURE_ABNORMAL = 5,
    VOLTAGE_ABNORMAL = 6
};

// 互锁信号快照
struct InterlockSignals {
    bool emergency_stop_triggered;
    bool safety_door_open;
    bool pressure_abnormal;
    bool temperature_abnormal;
    bool voltage_abnormal;
    bool servo_alarm;

    InterlockSignals()
        : emergency_stop_triggered(false),
          safety_door_open(false),
          pressure_abnormal(false),
          temperature_abnormal(false),
          voltage_abnormal(false),
          servo_alarm(false) {}
};

// 互锁策略配置
struct InterlockPolicyConfig {
    bool enabled;
    bool check_emergency_stop;
    bool check_servo_alarm;
    bool check_safety_door;
    bool check_pressure;
    bool check_temperature;
    bool check_voltage;

    InterlockPolicyConfig()
        : enabled(true),
          check_emergency_stop(true),
          check_servo_alarm(true),
          check_safety_door(true),
          check_pressure(true),
          check_temperature(true),
          check_voltage(true) {}
};

// 互锁判定结果
struct InterlockDecision {
    bool triggered;
    InterlockPriority priority;
    InterlockCause cause;
    const char* reason;

    InterlockDecision()
        : triggered(false),
          priority(InterlockPriority::LOW),
          cause(InterlockCause::NONE),
          reason("") {}
};

}  // namespace ValueObjects
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
