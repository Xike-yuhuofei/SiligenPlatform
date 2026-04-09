#pragma once

// Compatibility surface: prefer the runtime-execution canonical contract when visible,
// but keep a local mirror for consumers that only receive workflow public includes.
#if __has_include("runtime_execution/contracts/safety/InterlockTypes.h")
#include "runtime_execution/contracts/safety/InterlockTypes.h"
#else
#include "shared/types/Types.h"

namespace Siligen::Domain::Safety::ValueObjects {

using Siligen::Shared::Types::int8;

enum class InterlockPriority : int8 {
    CRITICAL = 0,
    HIGH = 1,
    MEDIUM = 2,
    LOW = 3
};

enum class InterlockCause : int8 {
    NONE = 0,
    EMERGENCY_STOP = 1,
    SERVO_ALARM = 2,
    SAFETY_DOOR_OPEN = 3,
    PRESSURE_ABNORMAL = 4,
    TEMPERATURE_ABNORMAL = 5,
    VOLTAGE_ABNORMAL = 6
};

struct InterlockSignals {
    bool emergency_stop_triggered = false;
    bool safety_door_open = false;
    bool pressure_abnormal = false;
    bool temperature_abnormal = false;
    bool voltage_abnormal = false;
    bool servo_alarm = false;
};

struct InterlockPolicyConfig {
    bool enabled = true;
    bool check_emergency_stop = true;
    bool check_servo_alarm = true;
    bool check_safety_door = true;
    bool check_pressure = true;
    bool check_temperature = true;
    bool check_voltage = true;
};

struct InterlockDecision {
    bool triggered = false;
    InterlockPriority priority = InterlockPriority::LOW;
    InterlockCause cause = InterlockCause::NONE;
    const char* reason = "";
};

}  // namespace Siligen::Domain::Safety::ValueObjects
#endif
