#include "siligen/motion/safety/services/interlock_policy.h"

#include "siligen/shared/error.h"

namespace Siligen::Motion::Safety::Services {
namespace {

constexpr const char* kReasonEmergencyStop = "急停按钮触发";
constexpr const char* kReasonServoAlarm = "伺服驱动器报警";
constexpr const char* kReasonSafetyDoorOpen = "安全门打开";
constexpr const char* kReasonPressureAbnormal = "气压超出安全范围";
constexpr const char* kReasonTemperatureAbnormal = "温度超出安全范围";
constexpr const char* kReasonVoltageAbnormal = "电压超出安全范围";
constexpr const char* kReasonNormal = "";
constexpr const char* kReasonJogEstop = "Cannot start jog: Emergency stop is active";
constexpr const char* kReasonJogPosLimit =
    "Cannot start jog in positive direction: Positive limit switch is already triggered";
constexpr const char* kReasonJogNegLimit =
    "Cannot start jog in negative direction: Negative limit switch is already triggered";

InterlockDecision BuildDecision(bool triggered,
                                InterlockPriority priority,
                                InterlockCause cause,
                                const char* reason) noexcept {
    InterlockDecision decision;
    decision.triggered = triggered;
    decision.priority = priority;
    decision.cause = cause;
    decision.reason = reason;
    return decision;
}

}  // namespace

Siligen::SharedKernel::Result<InterlockDecision> InterlockPolicy::EvaluateSignals(
    const InterlockSignals& signals,
    const InterlockPolicyConfig& config) noexcept {
    if (!config.enabled) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(false, InterlockPriority::kLow, InterlockCause::kNone, kReasonNormal));
    }

    if (config.check_emergency_stop && signals.emergency_stop_triggered) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kCritical, InterlockCause::kEmergencyStop, kReasonEmergencyStop));
    }
    if (config.check_servo_alarm && signals.servo_alarm) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kHigh, InterlockCause::kServoAlarm, kReasonServoAlarm));
    }
    if (config.check_safety_door && signals.safety_door_open) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kHigh, InterlockCause::kSafetyDoorOpen, kReasonSafetyDoorOpen));
    }
    if (config.check_pressure && signals.pressure_abnormal) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kLow, InterlockCause::kPressureAbnormal, kReasonPressureAbnormal));
    }
    if (config.check_temperature && signals.temperature_abnormal) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kLow, InterlockCause::kTemperatureAbnormal, kReasonTemperatureAbnormal));
    }
    if (config.check_voltage && signals.voltage_abnormal) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::kLow, InterlockCause::kVoltageAbnormal, kReasonVoltageAbnormal));
    }

    return Siligen::SharedKernel::Result<InterlockDecision>::Success(
        BuildDecision(false, InterlockPriority::kLow, InterlockCause::kNone, kReasonNormal));
}

Siligen::SharedKernel::Result<InterlockDecision> InterlockPolicy::Evaluate(
    const Ports::InterlockSignalPort& port,
    const InterlockPolicyConfig& config) noexcept {
    auto signals_result = port.ReadSignals();
    if (signals_result.IsError()) {
        return Siligen::SharedKernel::Result<InterlockDecision>::Failure(signals_result.GetError());
    }
    return EvaluateSignals(signals_result.Value(), config);
}

Siligen::SharedKernel::VoidResult InterlockPolicy::CheckAxisForJog(
    const AxisSafetyStatus& status,
    Siligen::SharedKernel::int16 direction,
    const char* error_source) noexcept {
    const char* source = (error_source != nullptr && error_source[0] != '\0') ? error_source : "InterlockPolicy";
    if (status.estop_active) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::EMERGENCY_STOP_ACTIVATED,
                kReasonJogEstop,
                source));
    }
    if (direction == 1 && status.hard_limit_positive) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::POSITION_OUT_OF_RANGE,
                kReasonJogPosLimit,
                source));
    }
    if (direction == -1 && status.hard_limit_negative) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::POSITION_OUT_OF_RANGE,
                kReasonJogNegLimit,
                source));
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult InterlockPolicy::CheckAxisForHoming(
    const AxisSafetyStatus& status,
    const char* axis_name,
    const char* error_source) noexcept {
    const char* source = (error_source != nullptr && error_source[0] != '\0') ? error_source : "InterlockPolicy";
    std::string axis_prefix = "轴";
    if (axis_name != nullptr && axis_name[0] != '\0') {
        axis_prefix += axis_name;
    }

    if (status.estop_active) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::EMERGENCY_STOP_ACTIVATED,
                axis_prefix + "处于急停状态，无法回零",
                source));
    }
    if (status.moving) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::INVALID_STATE,
                axis_prefix + "正在运动，无法回零",
                source));
    }
    if (status.servo_alarm) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::HARDWARE_ERROR,
                axis_prefix + "伺服报警，无法回零",
                source));
    }
    if (status.following_error) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::HARDWARE_ERROR,
                axis_prefix + "跟随误差，无法回零",
                source));
    }
    if (status.faulted || status.has_error) {
        return Siligen::SharedKernel::VoidResult::Failure(
            Siligen::SharedKernel::Error(
                Siligen::SharedKernel::ErrorCode::HARDWARE_ERROR,
                axis_prefix + "处于故障状态，无法回零",
                source));
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

bool InterlockPolicy::IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept {
    return positive_limit || negative_limit;
}

bool InterlockPolicy::IsSoftLimitTriggered(
    const AxisSafetyStatus& status,
    bool* positive_limit,
    bool* negative_limit) noexcept {
    if (positive_limit != nullptr) {
        *positive_limit = status.soft_limit_positive;
    }
    if (negative_limit != nullptr) {
        *negative_limit = status.soft_limit_negative;
    }
    return status.soft_limit_positive || status.soft_limit_negative;
}

}  // namespace Siligen::Motion::Safety::Services
