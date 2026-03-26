#pragma once

#include "siligen/shared/numeric_types.h"

namespace Siligen::Motion::Safety {

enum class InterlockPriority : Siligen::SharedKernel::int8 {
    kCritical = 0,
    kHigh = 1,
    kMedium = 2,
    kLow = 3,
};

enum class InterlockCause : Siligen::SharedKernel::int8 {
    kNone = 0,
    kEmergencyStop = 1,
    kServoAlarm = 2,
    kSafetyDoorOpen = 3,
    kPressureAbnormal = 4,
    kTemperatureAbnormal = 5,
    kVoltageAbnormal = 6,
};

struct InterlockSignals {
    bool emergency_stop_triggered = false;
    bool safety_door_open = false;
    bool pressure_abnormal = false;
    bool temperature_abnormal = false;
    bool voltage_abnormal = false;
    bool servo_alarm = false;
};

struct AxisSafetyStatus {
    bool estop_active = false;
    bool moving = false;
    bool faulted = false;
    bool has_error = false;
    bool following_error = false;
    bool soft_limit_positive = false;
    bool soft_limit_negative = false;
    bool hard_limit_positive = false;
    bool hard_limit_negative = false;
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
    InterlockPriority priority = InterlockPriority::kLow;
    InterlockCause cause = InterlockCause::kNone;
    const char* reason = "";
};

}  // namespace Siligen::Motion::Safety
