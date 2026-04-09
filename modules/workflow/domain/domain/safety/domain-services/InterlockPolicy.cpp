#include "InterlockPolicy.h"

#include "siligen/motion/safety/services/interlock_policy.h"

namespace Siligen {
namespace Domain {
namespace Safety {
namespace DomainServices {
namespace {

Siligen::SharedKernel::Error ToMotionError(const Error& error) {
    return Siligen::SharedKernel::Error(
        static_cast<Siligen::SharedKernel::ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

Error ToDomainError(const Siligen::SharedKernel::Error& error) {
    return Error(
        static_cast<ErrorCode>(static_cast<int>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

Siligen::Motion::Safety::InterlockSignals ToMotionSignals(const InterlockSignals& signals) {
    Siligen::Motion::Safety::InterlockSignals converted;
    converted.emergency_stop_triggered = signals.emergency_stop_triggered;
    converted.safety_door_open = signals.safety_door_open;
    converted.pressure_abnormal = signals.pressure_abnormal;
    converted.temperature_abnormal = signals.temperature_abnormal;
    converted.voltage_abnormal = signals.voltage_abnormal;
    converted.servo_alarm = signals.servo_alarm;
    return converted;
}

Siligen::Motion::Safety::InterlockPolicyConfig ToMotionConfig(const InterlockPolicyConfig& config) {
    Siligen::Motion::Safety::InterlockPolicyConfig converted;
    converted.enabled = config.enabled;
    converted.check_emergency_stop = config.check_emergency_stop;
    converted.check_servo_alarm = config.check_servo_alarm;
    converted.check_safety_door = config.check_safety_door;
    converted.check_pressure = config.check_pressure;
    converted.check_temperature = config.check_temperature;
    converted.check_voltage = config.check_voltage;
    return converted;
}

Siligen::Motion::Safety::AxisSafetyStatus ToMotionAxisSafetyStatus(const MotionStatus& status) {
    Siligen::Motion::Safety::AxisSafetyStatus converted;
    converted.estop_active = status.state == MotionState::ESTOP;
    converted.moving = status.state == MotionState::MOVING;
    converted.faulted = status.state == MotionState::FAULT;
    converted.has_error = status.has_error;
    converted.following_error = status.following_error;
    converted.soft_limit_positive = status.soft_limit_positive;
    converted.soft_limit_negative = status.soft_limit_negative;
    converted.hard_limit_positive = status.hard_limit_positive;
    converted.hard_limit_negative = status.hard_limit_negative;
    converted.servo_alarm = status.servo_alarm;
    return converted;
}

InterlockDecision ToDomainDecision(const Siligen::Motion::Safety::InterlockDecision& decision) {
    InterlockDecision converted;
    converted.triggered = decision.triggered;
    converted.priority = static_cast<InterlockPriority>(static_cast<int>(decision.priority));
    converted.cause = static_cast<InterlockCause>(static_cast<int>(decision.cause));
    converted.reason = decision.reason;
    return converted;
}

Result<InterlockDecision> ToDomainDecisionResult(
    const Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockDecision>& result) {
    if (result.IsError()) {
        return Result<InterlockDecision>::Failure(ToDomainError(result.GetError()));
    }
    return Result<InterlockDecision>::Success(ToDomainDecision(result.Value()));
}

Result<void> ToDomainVoidResult(const Siligen::SharedKernel::VoidResult& result) {
    if (result.IsSuccess()) {
        return Result<void>::Success();
    }
    return Result<void>::Failure(ToDomainError(result.GetError()));
}

class InterlockSignalPortAdapter final : public Siligen::Motion::Safety::Ports::InterlockSignalPort {
   public:
    explicit InterlockSignalPortAdapter(const IInterlockSignalPort& legacy) noexcept
        : legacy_(legacy) {}

    Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals> ReadSignals() const noexcept override {
        const auto result = legacy_.ReadSignals();
        if (result.IsError()) {
            return Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals>::Failure(
                ToMotionError(result.GetError()));
        }
        return Siligen::SharedKernel::Result<Siligen::Motion::Safety::InterlockSignals>::Success(
            ToMotionSignals(result.Value()));
    }

   private:
    const IInterlockSignalPort& legacy_;
};

}  // namespace

Result<InterlockDecision> InterlockPolicy::EvaluateSignals(const InterlockSignals& signals,
                                                           const InterlockPolicyConfig& config) noexcept {
    return ToDomainDecisionResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::EvaluateSignals(
            ToMotionSignals(signals),
            ToMotionConfig(config)));
}

Result<InterlockDecision> InterlockPolicy::Evaluate(const IInterlockSignalPort& port,
                                                    const InterlockPolicyConfig& config) noexcept {
    InterlockSignalPortAdapter adapter(port);
    return ToDomainDecisionResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::Evaluate(adapter, ToMotionConfig(config)));
}

Result<void> InterlockPolicy::CheckAxisForJog(const MotionStatus& status,
                                              int16_t direction,
                                              const char* error_source) noexcept {
    return ToDomainVoidResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::CheckAxisForJog(
            ToMotionAxisSafetyStatus(status),
            static_cast<Siligen::SharedKernel::int16>(direction),
            error_source));
}

Result<void> InterlockPolicy::CheckAxisForHoming(const MotionStatus& status,
                                                 const char* axis_name,
                                                 const char* error_source) noexcept {
    return ToDomainVoidResult(
        Siligen::Motion::Safety::Services::InterlockPolicy::CheckAxisForHoming(
            ToMotionAxisSafetyStatus(status),
            axis_name,
            error_source));
}

bool InterlockPolicy::IsHardLimitTriggered(bool positive_limit, bool negative_limit) noexcept {
    return Siligen::Motion::Safety::Services::InterlockPolicy::IsHardLimitTriggered(
        positive_limit,
        negative_limit);
}

bool InterlockPolicy::IsSoftLimitTriggered(const MotionStatus& status,
                                           bool* positive_limit,
                                           bool* negative_limit) noexcept {
    return Siligen::Motion::Safety::Services::InterlockPolicy::IsSoftLimitTriggered(
        ToMotionAxisSafetyStatus(status),
        positive_limit,
        negative_limit);
}

}  // namespace DomainServices
}  // namespace Safety
}  // namespace Domain
}  // namespace Siligen
