#include "domain/safety/domain-services/InterlockPolicy.h"

namespace Siligen::Domain::Safety::DomainServices {
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

const char* ResolveErrorSource(const char* error_source) noexcept {
    return (error_source != nullptr && error_source[0] != '\0') ? error_source : "InterlockPolicy";
}

}  // namespace

Result<InterlockDecision> InterlockPolicy::EvaluateSignals(const InterlockSignals& signals,
                                                           const InterlockPolicyConfig& config) noexcept {
    if (!config.enabled) {
        return Result<InterlockDecision>::Success(
            BuildDecision(false, InterlockPriority::LOW, InterlockCause::NONE, kReasonNormal));
    }

    if (config.check_emergency_stop && signals.emergency_stop_triggered) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::CRITICAL, InterlockCause::EMERGENCY_STOP, kReasonEmergencyStop));
    }
    if (config.check_servo_alarm && signals.servo_alarm) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::HIGH, InterlockCause::SERVO_ALARM, kReasonServoAlarm));
    }
    if (config.check_safety_door && signals.safety_door_open) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::HIGH, InterlockCause::SAFETY_DOOR_OPEN, kReasonSafetyDoorOpen));
    }
    if (config.check_pressure && signals.pressure_abnormal) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::LOW, InterlockCause::PRESSURE_ABNORMAL, kReasonPressureAbnormal));
    }
    if (config.check_temperature && signals.temperature_abnormal) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::LOW, InterlockCause::TEMPERATURE_ABNORMAL, kReasonTemperatureAbnormal));
    }
    if (config.check_voltage && signals.voltage_abnormal) {
        return Result<InterlockDecision>::Success(
            BuildDecision(true, InterlockPriority::LOW, InterlockCause::VOLTAGE_ABNORMAL, kReasonVoltageAbnormal));
    }

    return Result<InterlockDecision>::Success(
        BuildDecision(false, InterlockPriority::LOW, InterlockCause::NONE, kReasonNormal));
}

Result<InterlockDecision> InterlockPolicy::Evaluate(const IInterlockSignalPort& port,
                                                    const InterlockPolicyConfig& config) noexcept {
    const auto signals_result = port.ReadSignals();
    if (signals_result.IsError()) {
        return Result<InterlockDecision>::Failure(signals_result.GetError());
    }
    return EvaluateSignals(signals_result.Value(), config);
}

Result<void> InterlockPolicy::CheckAxisForJog(const MotionStatus& status,
                                              int16_t direction,
                                              const char* error_source) noexcept {
    const char* source = ResolveErrorSource(error_source);
    if (status.state == MotionState::ESTOP) {
        return Result<void>::Failure(
            Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, kReasonJogEstop, source));
    }
    if (direction == 1 && status.hard_limit_positive) {
        return Result<void>::Failure(
            Error(ErrorCode::POSITION_OUT_OF_RANGE, kReasonJogPosLimit, source));
    }
    if (direction == -1 && status.hard_limit_negative) {
        return Result<void>::Failure(
            Error(ErrorCode::POSITION_OUT_OF_RANGE, kReasonJogNegLimit, source));
    }
    return Result<void>::Success();
}

Result<void> InterlockPolicy::CheckAxisForHoming(const MotionStatus& status,
                                                 const char* axis_name,
                                                 const char* error_source) noexcept {
    const char* source = ResolveErrorSource(error_source);
    std::string axis_prefix = "轴";
    if (axis_name != nullptr && axis_name[0] != '\0') {
        axis_prefix += axis_name;
    }

    if (status.state == MotionState::ESTOP) {
        return Result<void>::Failure(
            Error(ErrorCode::EMERGENCY_STOP_ACTIVATED, axis_prefix + "处于急停状态，无法回零", source));
    }
    if (status.state == MotionState::MOVING) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_STATE, axis_prefix + "正在运动，无法回零", source));
    }
    if (status.servo_alarm) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_ERROR, axis_prefix + "伺服报警，无法回零", source));
    }
    if (status.following_error) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_ERROR, axis_prefix + "跟随误差，无法回零", source));
    }
    if (status.has_error || status.state == MotionState::FAULT) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_ERROR, axis_prefix + "处于故障状态，无法回零", source));
    }
    return Result<void>::Success();
}

bool InterlockPolicy::IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept {
    return positive_limit || negative_limit;
}

bool InterlockPolicy::IsSoftLimitTriggered(const MotionStatus& status,
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

}  // namespace Siligen::Domain::Safety::DomainServices
